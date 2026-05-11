import 'package:flutter/material.dart';
import 'sidebar.dart';
import 'video_area.dart';


// 主界面，包含侧边栏和视频区域
class MainScreen extends StatelessWidget {
  const MainScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      body: Row(
        children: const [
            SideBar(),// 左侧
            Expanded(        // 右侧 (使用 Expanded 让它填满剩余空间)
            child: VideoArea(),
          ),
        ],
        // children: const [
        //     Expanded(
        //       child: VideoArea(),
        //     ),
        //     SideBar(),
        // ],
      ),
    );
  }
}