import 'package:flutter/material.dart';


// 侧边栏
class SideBar extends StatelessWidget {
  const SideBar({super.key});

  @override
  Widget build(BuildContext context) {
    return Container(
      width: 80, // 固定宽度
      color: Colors.grey[900], // 深色背景
      child: const Column(
        children: [
          SizedBox(height: 20),
          Icon(Icons.video_library, color: Colors.white, size: 30),
          SizedBox(height: 20),
          // 这里可以放更多的菜单图标

          //中间空出来
          Spacer(),
          //放一个设置图标在底部
          Icon(Icons.settings, color: Colors.white, size: 30), 
          SizedBox(height: 20),
        ],
      ),
    );
  }
}