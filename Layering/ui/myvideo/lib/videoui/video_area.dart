import 'package:flutter/material.dart';
// 导入控制栏
import 'control_bar.dart'; 

class VideoArea extends StatelessWidget {
  const VideoArea({super.key});

  @override
  Widget build(BuildContext context) {
    return Container(
      color: Colors.black, // 视频区域背景色
      width: double.infinity, // 占据剩余所有宽度
      child: Stack(
        children: [
          // 1. 视频占位符 (中间的文字)
          const Center(
            child: Text(
              "视频画面区域",
              style: TextStyle(color: Colors.grey, fontSize: 24),
            ),
          ),
          
          // 2. 底部控制栏 (使用 Positioned 定位在底部)
          Positioned(
            bottom: 0,
            left: 0,
            right: 0,
            child: Container(
              color: Colors.black54, // 半透明背景，防止图标和视频混在一起看不清
              child: const ControlBar(),
            ),
          ),
        ],
      ),
    );
  }
}