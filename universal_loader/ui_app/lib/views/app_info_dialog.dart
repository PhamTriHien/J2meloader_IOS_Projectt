import 'dart:io';
import 'package:flutter/material.dart';
import '../bridge/platform_channel.dart';

class AppInfoDialog extends StatelessWidget {
  final int id;
  final String title;
  final String vendor;
  final String version;
  final String path;
  final String imagePath;
  final int installedTimestamp;
  final int lastPlayedTimestamp;
  final int playCount;
  final VoidCallback? onRmsCleared;

  const AppInfoDialog({
    super.key,
    required this.id,
    required this.title,
    required this.vendor,
    required this.version,
    required this.path,
    required this.imagePath,
    this.installedTimestamp = 0,
    this.lastPlayedTimestamp = 0,
    this.playCount = 0,
    this.onRmsCleared,
  });

  String _formatBytes(int bytes) {
    if (bytes < 1024) return "$bytes B";
    if (bytes < 1024 * 1024) return "${(bytes / 1024).toStringAsFixed(1)} KB";
    return "${(bytes / (1024 * 1024)).toStringAsFixed(2)} MB";
  }

  String _formatDate(int timestampMs) {
    if (timestampMs <= 0) return "Chưa từng chơi";
    final dt = DateTime.fromMillisecondsSinceEpoch(timestampMs);
    final day = dt.day.toString().padLeft(2, '0');
    final month = dt.month.toString().padLeft(2, '0');
    final year = dt.year;
    final hour = dt.hour.toString().padLeft(2, '0');
    final minute = dt.minute.toString().padLeft(2, '0');
    final second = dt.second.toString().padLeft(2, '0');
    return "$day/$month/$year $hour:$minute:$second";
  }

  int _calculateDirectorySize(Directory dir) {
    if (!dir.existsSync()) return 0;
    int total = 0;
    try {
      for (final entity in dir.listSync(recursive: true, followLinks: false)) {
        if (entity is File) {
          total += entity.lengthSync();
        }
      }
    } catch (_) {}
    return total;
  }

  File _resolveIconFile() {
    if (imagePath.isNotEmpty) {
      final f1 = File(imagePath);
      if (f1.existsSync()) return f1;
      final f2 = File('./universal_rms/apps/$path/$imagePath');
      if (f2.existsSync()) return f2;
    }
    return File('./universal_rms/apps/$path/icon.png');
  }

  void _openFolderOnDesktop(BuildContext context) {
    final appDir = Directory('${Directory.current.path}/universal_rms/apps/$path');
    if (!appDir.existsSync()) {
      appDir.createSync(recursive: true);
    }
    if (Platform.isWindows) {
      Process.run('explorer.exe', [appDir.path.replaceAll('/', '\\')]);
    } else if (Platform.isMacOS) {
      Process.run('open', [appDir.path]);
    } else if (Platform.isLinux) {
      Process.run('xdg-open', [appDir.path]);
    }
  }

  Future<void> _exportJar(BuildContext context) async {
    final jarFile = File('./universal_rms/apps/$path/app.jar');
    if (!jarFile.existsSync()) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text("Không tìm thấy tệp app.jar để xuất!"), backgroundColor: Colors.redAccent),
      );
      return;
    }

    String exportDir = Directory.current.path;
    if (Platform.isWindows) {
      final userProfile = Platform.environment['USERPROFILE'];
      if (userProfile != null && Directory('$userProfile\\Downloads').existsSync()) {
        exportDir = '$userProfile\\Downloads';
      }
    } else if (Platform.isMacOS || Platform.isLinux) {
      final home = Platform.environment['HOME'];
      if (home != null && Directory('$home/Downloads').existsSync()) {
        exportDir = '$home/Downloads';
      }
    }

    final safeName = title.replaceAll(RegExp(r'[\\/:*?"<>|]'), '_');
    final targetPath = '$exportDir/${safeName}_v$version.jar';
    try {
      if (J2mePlatform.isMobile) {
        final destination = await J2mePlatform.exportFile(
            jarFile.absolute.path, '${safeName}_v$version.jar');
        if (destination == null || !context.mounted) return;
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Đã xuất tệp JAR')),
        );
        return;
      }
      await jarFile.copy(targetPath);
      if (!context.mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text("Đã xuất tệp JAR thành công: $targetPath"),
          backgroundColor: const Color(0xFF0284C7),
          duration: const Duration(seconds: 3),
        ),
      );
    } catch (e) {
      if (!context.mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text("Xuất tệp thất bại: $e"), backgroundColor: Colors.redAccent),
      );
    }
  }

  Widget _buildInfoRow(String label, String value, {IconData? icon, Color? valueColor}) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 2.5),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          if (icon != null) ...[
            Icon(icon, size: 14, color: const Color(0xFF38BDF8)),
            const SizedBox(width: 6),
          ],
          SizedBox(
            width: 110,
            child: Text(
              label,
              style: const TextStyle(color: Colors.white60, fontSize: 11.5),
            ),
          ),
          Expanded(
            child: Text(
              value,
              style: TextStyle(
                color: valueColor ?? Colors.white,
                fontSize: 11.5,
                fontWeight: FontWeight.w600,
              ),
            ),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final media = MediaQuery.of(context).size;
    final dialogWidth = (media.width - 32).clamp(260.0, 440.0);
    final jarFile = File('./universal_rms/apps/$path/app.jar');
    final int jarSize = jarFile.existsSync() ? jarFile.lengthSync() : 0;
    final int rmsSize = _calculateDirectorySize(Directory('./universal_rms/data/$path'));
    final iconFile = _resolveIconFile();

    return AlertDialog(
      backgroundColor: const Color(0xFF131B2E),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
      insetPadding: const EdgeInsets.symmetric(horizontal: 10, vertical: 12),
      titlePadding: const EdgeInsets.fromLTRB(14, 12, 10, 4),
      contentPadding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
      title: Row(
        children: [
          Container(
            width: 32,
            height: 32,
            decoration: BoxDecoration(
              color: const Color(0xFF1E293B),
              borderRadius: BorderRadius.circular(6),
            ),
            clipBehavior: Clip.antiAlias,
            alignment: Alignment.center,
            child: iconFile.existsSync()
                ? Image.file(iconFile, width: 32, height: 32, fit: BoxFit.contain)
                : const Icon(Icons.sports_esports, color: Color(0xFF38BDF8), size: 18),
          ),
          const SizedBox(width: 8),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  title,
                  style: const TextStyle(color: Colors.white, fontSize: 13.5, fontWeight: FontWeight.bold),
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                ),
                Text(
                  "$vendor • v$version",
                  style: const TextStyle(color: Colors.white60, fontSize: 11),
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                ),
              ],
            ),
          ),
          IconButton(
            visualDensity: VisualDensity.compact,
            padding: EdgeInsets.zero,
            constraints: const BoxConstraints(minWidth: 28, minHeight: 28),
            icon: const Icon(Icons.close, color: Colors.white60, size: 18),
            onPressed: () => Navigator.pop(context),
          ),
        ],
      ),
      content: SizedBox(
        width: dialogWidth,
        child: SingleChildScrollView(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Divider(color: Color(0xFF334155), height: 12),
              _buildInfoRow("Mã gói (Folder):", path, icon: Icons.folder),
              _buildInfoRow("Dung lượng JAR:", _formatBytes(jarSize), icon: Icons.file_present),
              _buildInfoRow("Dữ liệu lưu (RMS):", _formatBytes(rmsSize), icon: Icons.storage, valueColor: const Color(0xFF38BDF8)),
              _buildInfoRow("Số lần đã chơi:", "$playCount lần", icon: Icons.play_circle_outline),
              _buildInfoRow("Ngày cài đặt:", _formatDate(installedTimestamp), icon: Icons.calendar_today),
              _buildInfoRow("Lần chơi gần nhất:", _formatDate(lastPlayedTimestamp), icon: Icons.history),
              const SizedBox(height: 8),
              Container(
                padding: const EdgeInsets.all(8),
                decoration: BoxDecoration(
                  color: const Color(0xFF0F172A),
                  borderRadius: BorderRadius.circular(6),
                  border: Border.all(color: const Color(0xFF1E293B)),
                ),
                child: Row(
                  children: [
                    const Icon(Icons.location_on, size: 14, color: Color(0xFF64748B)),
                    const SizedBox(width: 6),
                    Expanded(
                      child: Text(
                        "${Directory.current.path}/universal_rms/apps/$path",
                        style: const TextStyle(color: Color(0xFF94A3B8), fontSize: 10.5, fontFamily: 'monospace'),
                        maxLines: 2,
                        overflow: TextOverflow.ellipsis,
                      ),
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 8),
              Wrap(
                spacing: 6,
                runSpacing: 6,
                children: [
                  if (!J2mePlatform.isMobile)
                  OutlinedButton.icon(
                    style: OutlinedButton.styleFrom(
                      foregroundColor: const Color(0xFF38BDF8),
                      side: const BorderSide(color: Color(0xFF38BDF8)),
                      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
                      visualDensity: VisualDensity.compact,
                    ),
                    icon: const Icon(Icons.folder_open, size: 14),
                    label: const Text("Mở thư mục", style: TextStyle(fontSize: 11)),
                    onPressed: () => _openFolderOnDesktop(context),
                  ),
                  OutlinedButton.icon(
                    style: OutlinedButton.styleFrom(
                      foregroundColor: const Color(0xFF38BDF8),
                      side: const BorderSide(color: Color(0xFF38BDF8)),
                      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
                      visualDensity: VisualDensity.compact,
                    ),
                    icon: const Icon(Icons.download, size: 14),
                    label: const Text("Xuất JAR", style: TextStyle(fontSize: 11)),
                    onPressed: () => _exportJar(context),
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
      actionsPadding: const EdgeInsets.fromLTRB(12, 0, 12, 10),
      actions: [
        ElevatedButton(
          style: ElevatedButton.styleFrom(
            backgroundColor: const Color(0xFF0284C7),
            padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 6),
            visualDensity: VisualDensity.compact,
          ),
          onPressed: () => Navigator.pop(context),
          child: const Text("Đóng", style: TextStyle(fontSize: 12)),
        ),
      ],
    );
  }
}
