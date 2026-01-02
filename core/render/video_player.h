//
//  video_player.h
//  FFMPEG OpenGL
//
//  Created by Dmitri Wamback on 2026-01-01.
//

#ifndef video_player_h
#define video_player_h

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
}

#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#include <GL/glew.h>

class Texture;

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    bool load(const char* path);
    void play(bool loop);
    void stop();

    void updateTexture(Texture& texture);

    int  getWidth()  const { return width; }
    int  getHeight() const { return height; }

    double getAudioClock() const { return audioClock.load(); }

private:
    std::thread audioThread;
    std::thread videoThread;
    std::atomic<bool> isRunning;
    std::atomic<double> audioClock;
    std::mutex frameMutex;

    uint8_t* frameData;
    int width  = 0;
    int height = 0;
    int frameStrideBytes = 0;
    bool shouldLoop = false;
    const char* path;

    std::atomic<double>  audioBasePTS;
    std::atomic<int64_t> totalSamplesQueued;
    std::atomic<int64_t> totalSamplesPlayed;

    ALCdevice*  alDevice  = nullptr;
    ALCcontext* alContext = nullptr;
    ALuint      alSource  = 0;
    ALuint      alBuffers[4];
    int         bufferSamples[4];

    static const int OUT_CHANNELS    = 2;
    static const int OUT_SAMPLE_RATE = 44100;
    static const int BUFFER_FRAMES   = 4096;
    static const int BUFFER_BYTES    = BUFFER_FRAMES * OUT_CHANNELS * 2;

    void audioLoop(const char* path);
    void videoLoop(const char* path, bool loop);

    void initOpenAL();
    void cleanupOpenAL();
};

VideoPlayer::VideoPlayer() : isRunning(false),  audioClock(0.0),        frameData(nullptr),
                             audioBasePTS(0.0), totalSamplesQueued(0),  totalSamplesPlayed(0) {
    for (int i = 0; i < 4; ++i) {
        alBuffers[i] = 0;
        bufferSamples[i] = 0;
    }
}

VideoPlayer::~VideoPlayer() {
    stop();
    if (frameData) {
        free(frameData);
        frameData = nullptr;
    }
    cleanupOpenAL();
}

void VideoPlayer::initOpenAL() {
    alDevice = alcOpenDevice(nullptr);
    if (!alDevice) throw std::runtime_error("Failed to open OpenAL device");

    alContext = alcCreateContext(alDevice, nullptr);
    if (!alContext) throw std::runtime_error("Failed to create OpenAL context");

    alcMakeContextCurrent(alContext);

    alGenSources(1, &alSource);
    alGenBuffers(4, alBuffers);
    for (int i = 0; i < 4; ++i) bufferSamples[i] = 0;
}

void VideoPlayer::cleanupOpenAL() {
    if (alSource) {
        alDeleteSources(1, &alSource);
        alSource = 0;
    }
    alDeleteBuffers(4, alBuffers);

    if (alContext) {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(alContext);
        alContext = nullptr;
    }
    if (alDevice) {
        alcCloseDevice(alDevice);
        alDevice = nullptr;
    }
}

bool VideoPlayer::load(const char* path) {

    avformat_network_init();
    
    this->path = path;

    AVFormatContext* formatCtx = avformat_alloc_context();
    if (avformat_open_input(&formatCtx, path, nullptr, nullptr) != 0) {
        std::cerr << "Failed to open video: " << path << "\n";
        return false;
    }
    avformat_find_stream_info(formatCtx, nullptr);

    int videoStream = -1;
    for (uint32_t i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            break;
        }
    }
    if (videoStream == -1) {
        std::cerr << "No video stream\n";
        avformat_close_input(&formatCtx);
        return false;
    }

    AVCodecParameters* codecParameters  = formatCtx->streams[videoStream]->codecpar;
    const AVCodec* pCodec               = avcodec_find_decoder(codecParameters->codec_id);
    AVCodecContext* codecCtx            = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(codecCtx, codecParameters);
    avcodec_open2(codecCtx, pCodec, nullptr);

    width  = codecCtx->width;
    height = codecCtx->height;

    if (!frameData) {
        frameStrideBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 1);
        frameData = (uint8_t*)malloc(frameStrideBytes);
    }

    AVFrame* frame = av_frame_alloc();
    AVFrame* frameRGB = av_frame_alloc();
    SwsContext* swsCtx = sws_getContext(width, height, codecCtx->pix_fmt,
                                        width, height, AV_PIX_FMT_RGB24,
                                        SWS_BILINEAR, nullptr, nullptr, nullptr);

    uint8_t* buffer = (uint8_t*)av_malloc(frameStrideBytes);
    av_image_fill_arrays(frameRGB->data, frameRGB->linesize,
                         buffer, AV_PIX_FMT_RGB24, width, height, 1);

    AVPacket* packet = av_packet_alloc();

    while (av_read_frame(formatCtx, packet) >= 0) {
        if (packet->stream_index != videoStream) {
            av_packet_unref(packet);
            continue;
        }
        if (avcodec_send_packet(codecCtx, packet) != 0) {
            av_packet_unref(packet);
            continue;
        }
        if (avcodec_receive_frame(codecCtx, frame) != 0) {
            av_packet_unref(packet);
            continue;
        }

        sws_scale(swsCtx, frame->data, frame->linesize, 0, height, frameRGB->data, frameRGB->linesize);
        {
            std::lock_guard<std::mutex> lock(frameMutex);
            memcpy(frameData, frameRGB->data[0], frameStrideBytes);
        }
        av_packet_unref(packet);
        break;
    }

    av_frame_free(&frame);
    av_frame_free(&frameRGB);
    av_packet_free(&packet);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&formatCtx);
    av_free(buffer);

    initOpenAL();

    return true;
}

void VideoPlayer::play(bool loop = true) {
    if (isRunning.load()) return;
    
    shouldLoop = loop;
    
    isRunning = true;
    audioClock.store(0.0);
    audioBasePTS.store(0.0);
    totalSamplesQueued.store(0);
    totalSamplesPlayed.store(0);

    audioThread = std::thread(&VideoPlayer::audioLoop, this, path);
    videoThread = std::thread(&VideoPlayer::videoLoop, this, path, shouldLoop);
}

void VideoPlayer::stop() {
    if (!isRunning.load()) return;
    isRunning = false;

    if (videoThread.joinable()) videoThread.join();
    if (audioThread.joinable()) audioThread.join();
}

void VideoPlayer::updateTexture(Texture& texture) {
    std::lock_guard<std::mutex> lock(frameMutex);
    if (frameData) {
        texture.UpdateWithBytes(frameData);
    }
}

void VideoPlayer::audioLoop(const char* path) {
    AVFormatContext* fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, path, nullptr, nullptr) != 0) return;
    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) return;

    int audioStream = -1;
    for (uint32_t i = 0; i < fmtCtx->nb_streams; i++) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStream = i;
            break;
        }
    }
    if (audioStream == -1) return;

    AVStream* aStream = fmtCtx->streams[audioStream];
    AVCodecParameters* codecParams = aStream->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecParams->codec_id);
    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, codecParams);
    avcodec_open2(codecCtx, codec, nullptr);

    AVChannelLayout inLayout, outLayout;
    
    if (codecCtx->ch_layout.nb_channels > 0) {
        inLayout = codecCtx->ch_layout;
    }
    else {
        av_channel_layout_default(&inLayout, codecParams->ch_layout.nb_channels);
    }
    av_channel_layout_default(&outLayout, OUT_CHANNELS);

    SwrContext* swr = nullptr;
    swr_alloc_set_opts2(
        &swr,
        &outLayout,
        AV_SAMPLE_FMT_S16,
        OUT_SAMPLE_RATE,
        &inLayout,
        codecCtx->sample_fmt,
        codecCtx->sample_rate,
        0, nullptr
    );
    if (!swr || swr_init(swr) < 0) return;

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame   = av_frame_alloc();
    std::vector<uint8_t> audioBuffer(BUFFER_BYTES);
    uint8_t* outArray[1] = { audioBuffer.data() };

    int nextBuffer = 0;
    bool firstPTSSet = false;
    audioBasePTS.store(0.0);
    totalSamplesQueued.store(0);
    totalSamplesPlayed.store(0);
    for (int i = 0; i < 4; ++i) bufferSamples[i] = 0;

    while (nextBuffer < 4 && isRunning) {
        if (av_read_frame(fmtCtx, packet) < 0) break;
        if (packet->stream_index != audioStream) { av_packet_unref(packet); continue; }
        if (avcodec_send_packet(codecCtx, packet) < 0) { av_packet_unref(packet); continue; }
        if (avcodec_receive_frame(codecCtx, frame) < 0) { av_packet_unref(packet); continue; }

        int converted = swr_convert(swr, outArray, BUFFER_FRAMES,
                                    (const uint8_t**)frame->data, frame->nb_samples);
        if (converted > 0) {
            if (!firstPTSSet) {
                double ptsSec;
                if (frame->best_effort_timestamp != AV_NOPTS_VALUE) {
                    ptsSec = frame->best_effort_timestamp * av_q2d(aStream->time_base);
                } else if (frame->pts != AV_NOPTS_VALUE) {
                    ptsSec = frame->pts * av_q2d(aStream->time_base);
                } else {
                    ptsSec = 0.0;
                }
                audioBasePTS.store(ptsSec);
                firstPTSSet = true;
            }

            int samples = converted;
            int bytes   = samples * OUT_CHANNELS * 2;

            alBufferData(alBuffers[nextBuffer], AL_FORMAT_STEREO16,
                         audioBuffer.data(), bytes, OUT_SAMPLE_RATE);
            alSourceQueueBuffers(alSource, 1, &alBuffers[nextBuffer]);

            bufferSamples[nextBuffer] = samples;
            totalSamplesQueued.fetch_add(samples);
            ++nextBuffer;
        }
        av_packet_unref(packet);
    }

    alSourcePlay(alSource);

    while (isRunning) {
        ALint processed = 0;
        alGetSourcei(alSource, AL_BUFFERS_PROCESSED, &processed);

        while (processed-- > 0 && isRunning) {
            ALuint buf;
            alSourceUnqueueBuffers(alSource, 1, &buf);

            int bufIndex = -1;
            for (int i = 0; i < 4; ++i) {
                if (alBuffers[i] == buf) { bufIndex = i; break; }
            }
            if (bufIndex >= 0) {
                totalSamplesPlayed.fetch_add(bufferSamples[bufIndex]);
                bufferSamples[bufIndex] = 0;
            }

            while (av_read_frame(fmtCtx, packet) >= 0 && isRunning) {
                if (packet->stream_index != audioStream) { av_packet_unref(packet); continue; }
                if (avcodec_send_packet(codecCtx, packet) < 0) { av_packet_unref(packet); continue; }
                if (avcodec_receive_frame(codecCtx, frame) < 0) { av_packet_unref(packet); continue; }

                int converted = swr_convert(swr, outArray, BUFFER_FRAMES,
                                            (const uint8_t**)frame->data, frame->nb_samples);
                av_packet_unref(packet);

                if (converted > 0) {
                    int samples = converted;
                    int bytes   = samples * OUT_CHANNELS * 2;

                    alBufferData(buf, AL_FORMAT_STEREO16,
                                 audioBuffer.data(), bytes, OUT_SAMPLE_RATE);
                    alSourceQueueBuffers(alSource, 1, &buf);

                    if (bufIndex >= 0) bufferSamples[bufIndex] = samples;
                    totalSamplesQueued.fetch_add(samples);
                    break;
                }
            }
        }

        ALint state;
        alGetSourcei(alSource, AL_SOURCE_STATE, &state);
        if (state != AL_PLAYING) alSourcePlay(alSource);

        ALint sampleOffset = 0;
        alGetSourcei(alSource, AL_SAMPLE_OFFSET, &sampleOffset);
        int64_t played = totalSamplesPlayed.load() + sampleOffset;
        double  secondsPlayed = (double)played / (double)OUT_SAMPLE_RATE;
        double  basePTS       = audioBasePTS.load();
        audioClock.store(basePTS + secondsPlayed);

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    alSourceStop(alSource);
    alSourcei(alSource, AL_BUFFER, 0);

    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmtCtx);
    swr_free(&swr);
}

void VideoPlayer::videoLoop(const char* path, bool loop) {
    
    AVFormatContext* formatCtx = avformat_alloc_context();
    if (avformat_open_input(&formatCtx, path, nullptr, nullptr) != 0) return;
    avformat_find_stream_info(formatCtx, nullptr);

    int videoStream = -1;
    for (uint32_t i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            break;
        }
    }
    if (videoStream == -1) return;

    AVCodecParameters* codecParameters = formatCtx->streams[videoStream]->codecpar;
    const AVCodec* pCodec = avcodec_find_decoder(codecParameters->codec_id);
    AVCodecContext* codecCtx = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(codecCtx, codecParameters);
    avcodec_open2(codecCtx, pCodec, nullptr);

    AVFrame* frame = av_frame_alloc();
    AVFrame* frameRGB = av_frame_alloc();
    SwsContext* swsCtx = sws_getContext(width, height, codecCtx->pix_fmt,
                                        width, height, AV_PIX_FMT_RGB24,
                                        SWS_BILINEAR, nullptr, nullptr, nullptr);

    uint8_t* buffer = (uint8_t*)av_malloc(frameStrideBytes);
    av_image_fill_arrays(frameRGB->data, frameRGB->linesize,
                        buffer, AV_PIX_FMT_RGB24, width, height, 1);

    AVPacket* packet = av_packet_alloc();

    const double SYNC_THRESHOLD = 0.15;
    bool firstVideoFrame = true;
    bool shouldLoop = loop;

    // EOF detector
    int consecutiveNoData = 0;
    const int MAX_NO_DATA = 50;

    while (isRunning.load()) {
        int ret = av_read_frame(formatCtx, packet);
        
        if (ret < 0 || packet->size == 0) {
            av_packet_unref(packet);
            consecutiveNoData++;
            
            if (consecutiveNoData >= MAX_NO_DATA) {
                if (shouldLoop) {
                    avcodec_flush_buffers(codecCtx);
                    
                    int64_t ts = 0;
                    int ret_seek = avformat_seek_file(formatCtx, videoStream,
                                                    -1, ts, ts + 1, AVSEEK_FLAG_BACKWARD);
                    if (ret_seek < 0) {
                        ret_seek = av_seek_frame(formatCtx, videoStream, 0, AVSEEK_FLAG_BACKWARD);
                        if (ret_seek < 0) {
                            std::cerr << "Seeks failed: " << ret_seek << std::endl;
                            break;
                        }
                    }
                    
                    AVPacket* drain_pkt = av_packet_alloc();
                    av_init_packet(drain_pkt);
                    drain_pkt->data = nullptr;
                    drain_pkt->size = 0;
                    
                    int drain_ret;
                    while ((drain_ret = avcodec_send_packet(codecCtx, drain_pkt)) >= 0) {
                        while (avcodec_receive_frame(codecCtx, frame) == 0) {
                            av_frame_unref(frame);
                        }
                    }
                    av_packet_free(&drain_pkt);
                    
                    firstVideoFrame = true;
                    consecutiveNoData = 0;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        
        consecutiveNoData = 0;

        if (packet->stream_index != videoStream) {
            av_packet_unref(packet);
            continue;
        }

        if (avcodec_send_packet(codecCtx, packet) != 0) {
            av_packet_unref(packet);
            continue;
        }
        if (avcodec_receive_frame(codecCtx, frame) != 0) {
            av_packet_unref(packet);
            continue;
        }

        sws_scale(swsCtx, frame->data, frame->linesize,
                  0, height, frameRGB->data, frameRGB->linesize);

        double framePTS = (frame->pts == AV_NOPTS_VALUE)
            ? frame->best_effort_timestamp * av_q2d(formatCtx->streams[videoStream]->time_base)
            : frame->pts * av_q2d(formatCtx->streams[videoStream]->time_base);

        while (true) {
            if (!isRunning.load()) break;

            double aclock = audioClock.load();
            double diff = framePTS - aclock;

            if (!firstVideoFrame && diff <= -SYNC_THRESHOLD) {
                goto drop_frame;
            }
            if (diff <= 0.0) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        {
            std::lock_guard<std::mutex> lock(frameMutex);
            memcpy(frameData, frameRGB->data[0], frameStrideBytes);
        }
        firstVideoFrame = false;

    drop_frame:
        av_frame_unref(frame);
        av_packet_unref(packet);
    }

    std::cerr << "Video loop exiting\n";
    
    av_frame_free(&frame);
    av_frame_free(&frameRGB);
    av_packet_free(&packet);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&formatCtx);
    av_free(buffer);
    sws_freeContext(swsCtx);
}



#endif /* video_player_h */
