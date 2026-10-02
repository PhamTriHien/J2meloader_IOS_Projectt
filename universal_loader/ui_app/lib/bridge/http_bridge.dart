import 'dart:ffi' as ffi;
import 'dart:io';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

typedef _HandlerC = ffi.Void Function(ffi.Int64);
typedef _SetHandlerC = ffi.Void Function(ffi.Pointer<ffi.NativeFunction<_HandlerC>>);
typedef _SetHandlerDart = void Function(ffi.Pointer<ffi.NativeFunction<_HandlerC>>);
typedef _InfoC = ffi.Size Function(ffi.Int64, ffi.Pointer<Utf8>, ffi.Size);
typedef _InfoDart = int Function(int, ffi.Pointer<Utf8>, int);
typedef _BodyC = ffi.Size Function(ffi.Int64, ffi.Pointer<ffi.Uint8>, ffi.Size);
typedef _BodyDart = int Function(int, ffi.Pointer<ffi.Uint8>, int);
typedef _CompleteC = ffi.Void Function(ffi.Int64, ffi.Pointer<Utf8>, ffi.Pointer<ffi.Uint8>, ffi.Size, ffi.Pointer<Utf8>);
typedef _CompleteDart = void Function(int, ffi.Pointer<Utf8>, ffi.Pointer<ffi.Uint8>, int, ffi.Pointer<Utf8>);

/// Performs the core's https:// requests with dart:io, which brings the platform TLS stack and trust store.
/// The core blocks the calling MIDlet thread until [_finish] answers.
class HostHttpBridge {
  HostHttpBridge._(ffi.DynamicLibrary lib)
      : _info = lib.lookupFunction<_InfoC, _InfoDart>('j2me_core_http_request_info'),
        _body = lib.lookupFunction<_BodyC, _BodyDart>('j2me_core_http_request_body'),
        _complete = lib.lookupFunction<_CompleteC, _CompleteDart>('j2me_core_http_complete') {
    _callable = ffi.NativeCallable<_HandlerC>.listener(_onRequest);
    lib.lookupFunction<_SetHandlerC, _SetHandlerDart>('j2me_core_set_http_handler')(_callable.nativeFunction);
  }

  static HostHttpBridge? _instance;

  static void install(ffi.DynamicLibrary lib) {
    _instance ??= HostHttpBridge._(lib);
  }

  final _InfoDart _info;
  final _BodyDart _body;
  final _CompleteDart _complete;
  late final ffi.NativeCallable<_HandlerC> _callable;
  // MIDlets send their own User-Agent when they care
  final HttpClient _client = HttpClient()
    ..connectionTimeout = const Duration(seconds: 15)
    ..userAgent = null;

  void _onRequest(int id) {
    _perform(id);
  }

  Future<void> _perform(int id) async {
    final info = _readInfo(id);
    if (info == null) return; // the core already gave up on this request
    final lines = info.split('\n');
    if (lines.length < 2) {
      _finish(id, error: 'Malformed request');
      return;
    }
    try {
      final request = await _client.openUrl(lines[0], Uri.parse(lines[1]));
      for (final line in lines.skip(2)) {
        final colon = line.indexOf(':');
        if (colon <= 0) continue;
        final name = line.substring(0, colon).trim();
        final lower = name.toLowerCase();
        if (lower == 'host' || lower == 'content-length' || lower == 'connection') continue;
        request.headers.set(name, line.substring(colon + 1).trim(), preserveHeaderCase: true);
      }
      final body = _readBody(id);
      if (body.isNotEmpty) {
        request.contentLength = body.length;
        request.add(body);
      }
      final response = await request.close().timeout(const Duration(seconds: 45));
      final bytes = await response
          .fold<BytesBuilder>(BytesBuilder(copy: false), (b, chunk) => b..add(chunk))
          .timeout(const Duration(seconds: 45));
      final data = bytes.takeBytes();
      final decompressed = response.compressionState == HttpClientResponseCompressionState.decompressed;
      final head = StringBuffer('HTTP/1.1 ${response.statusCode} ${response.reasonPhrase}');
      response.headers.forEach((name, values) {
        // The body handed over is already de-chunked (and un-gzipped when decompressed)
        if (name == 'transfer-encoding' || name == 'content-length') return;
        if (decompressed && name == 'content-encoding') return;
        for (final v in values) {
          head.write('\r\n$name: $v');
        }
      });
      head.write('\r\ncontent-length: ${data.length}');
      _finish(id, head: head.toString(), body: data);
    } catch (e) {
      _finish(id, error: 'HTTPS request failed: $e');
    }
  }

  String? _readInfo(int id) {
    final size = _info(id, ffi.nullptr, 0);
    if (size == 0) return null;
    final buf = malloc<ffi.Uint8>(size).cast<Utf8>();
    try {
      if (_info(id, buf, size) == 0) return null;
      return buf.toDartString();
    } finally {
      malloc.free(buf);
    }
  }

  Uint8List _readBody(int id) {
    final size = _body(id, ffi.nullptr, 0);
    if (size == 0) return Uint8List(0);
    final buf = malloc<ffi.Uint8>(size);
    try {
      _body(id, buf, size);
      return Uint8List.fromList(buf.asTypedList(size));
    } finally {
      malloc.free(buf);
    }
  }

  void _finish(int id, {String? head, Uint8List? body, String? error}) {
    final headPtr = head == null ? ffi.nullptr.cast<Utf8>() : head.toNativeUtf8();
    final errorPtr = error == null ? ffi.nullptr.cast<Utf8>() : error.toNativeUtf8();
    final length = body?.length ?? 0;
    final bodyPtr = length == 0 ? ffi.nullptr.cast<ffi.Uint8>() : malloc<ffi.Uint8>(length);
    if (length > 0) bodyPtr.asTypedList(length).setAll(0, body!);
    try {
      _complete(id, headPtr, bodyPtr, length, errorPtr);
    } finally {
      if (head != null) malloc.free(headPtr);
      if (error != null) malloc.free(errorPtr);
      if (length > 0) malloc.free(bodyPtr);
    }
  }
}
