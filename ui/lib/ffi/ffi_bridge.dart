import 'dart:ffi';
import 'dart:io';
import 'package:path/path.dart' as path;

class FFIBridge {
  static DynamicLibrary loadLibrary() {
    if (Platform.isWindows) {
      final currentDir = Directory(Platform.script.toFilePath()).parent;
      final dllPath = path.join(
        currentDir.path,
        '../../../build/client/service/libclient-service.dll',
      );

      return DynamicLibrary.open(dllPath);
    } else {
      return DynamicLibrary.open('libclient-service.so');
    }
  }
}
