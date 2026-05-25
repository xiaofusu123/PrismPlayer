#pragma once

#include <libavcodec/avcodec.h>

namespace Prism::engine {

/**
 * @struct audio_decoder_config
 * @brief 解码器配置信息
 * 
 * 结构体成员说明：
 * 
 * - codec_id 解码器ID，格式分别有：AAC、MP3（MPEG-1/2 Layer 3）、Opus（网络通信格式）、FLAC（无损压缩格式）、PCM
 * 
 * - bit_rate 比特率，低质量64-96kbps；标准质量128-192kbps；高质量256-320kbps；无损压缩700-1411kbps；原始PCM（CD）1411kbps及以上
 * 
 * - sample_rate 采样率，CD音频44.1kHz；DVD音频48kHz；母带96kHz或192kHz
 * 
 * - channels 声道数，单声道的通道数为1；立体声的通道数为2；多声道环绕声：5.1声道的通道数为6，7.1的通道数为8
 * 
 * - channel_layout 声道布局，分别有单声道、立体声、5.1环绕声、7.1环绕声
 * 
 * - sample_fmt 采样格式，别名位深，有16 bit、24 bit和32 bit
 */
struct audio_decoder_config {
    AVCodecID codec_id;         /**< 解码器ID */
    int bit_rate;               /**< 比特率（bps） */
    int sample_rate;            /**< 采样率（Hz） */
    int channels;               /**< 声道数 */
    uint64_t channel_layout;     /**< 声道布局 */
    AVSampleFormat sample_fmt;  /**< 采样格式（位深） */
};

/**
 * @struct audio_status_info
 * @brief 解码器状态信息，用于音频同步
 */
struct audio_decoder_info {
    uint64_t current_pts;  /**< 当前帧解码时间戳（ms） */
    uint16_t next_pts;     /**< 下一帧解码时间戳（ms） */
    uint64_t start_time;   /**< 流开始的时间 */
    uint64_t draution;     /**< 总时长 */

    /**
     * @enum Status
     * @brief 解码器状态信息
     */
    enum class Status {
        idle,              /**< 空闲 */
        running,           /**< 运行中 */
        stop               /**< 停止 */
    };

    Status status;         /**< 解码器状态 */
};

class AudioDecoder {
public:
    virtual ~AudioDecoder() = default;

    /**
    * @brief 音频解码器初始化
    * @param config 音频解码器配置
    */
    virtual bool init(const audio_decoder_config& config) = 0;
            
    /**
    * @brief 同步音频解码
    */
    virtual bool sync_decode() = 0;

    /**
    * @brief 异步音频解码
    */
    virtual bool async_decode() = 0;

    /**
    * @brief 重启音频解码器
    * 
    * 在音频解码器停止状态时，重新启动音频解码器
    */
    virtual bool restart() = 0;

    /**
    * @brief 停止音频解码
    * 
    * 停止正在工作的音频解码器
    */
    virtual bool stop() = 0;

    /**
    * @brief 刷新解码器
    * 
    * 清除解码器的缓存
    */
    virtual bool flush() = 0;

    /**
    * @brief 重置音频解码器
    * 
    * 将音频解码器重置为初始状态
    */
    virtual bool reset() = 0;

    /**
    * @brief 释放音频解码器资源
    */
    virtual bool free() = 0;

    /**
    * @brief 检查解码器是否在运行
    */
    virtual bool isRunning() = 0;

    /**
    * @brief 获取音频解码器配置信息
    */
    virtual audio_decoder_config get_config_info() const = 0;

    /**
    * @brief 获取音频解码器状态信息
    */
    virtual audio_decoder_info get_status_info() const = 0;
};

}

typedef struct student_s {

}student_t;
