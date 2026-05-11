import 'package:flutter/material.dart';

// 控制栏
class ControlBar extends StatelessWidget {
  const ControlBar({super.key});

  @override
  Widget build(BuildContext context) {
    return Container(
      height: 60,
      // 使用 Row 水平排列按钮
      child: Row(
        mainAxisAlignment: MainAxisAlignment.center, // 居中
        children: [
          IconButton(
            icon: const Icon(Icons.skip_previous, color: Colors.white, size: 30),
            onPressed: () {},
          ),
          const SizedBox(width: 20), // 间距
          IconButton(
            icon: const Icon(Icons.play_circle, color: Colors.white, size: 40),
            onPressed: () {},
          ),
          const SizedBox(width: 20),
          IconButton(
            icon: const Icon(Icons.skip_next, color: Colors.white, size: 30),
            onPressed: () {},
          ),
        ],
      ),
    );
  }
}