#pragma once
#include <memory>
#include "audio_file_manager.h"
#include "audio_database.h"

namespace Prism::persistence {

/**
 * @brief 创建音频文件管理器实例
 * @return std::unique_ptr<AudioFileManager> 文件管理器实例智能指针
 */
std::unique_ptr<AudioFileManager> create_audio_file_manager();

/**
 * @brief 创建音频数据库实例
 * @return std::unique_ptr<AudioDatabase> 数据库实例智能指针
 */
std::unique_ptr<AudioDatabase> create_audio_database();

}