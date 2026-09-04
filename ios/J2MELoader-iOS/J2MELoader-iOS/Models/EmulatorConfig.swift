import Foundation

public enum ResolutionPreset: String, Codable, CaseIterable {
    case res128x128 = "128 x 128 (Square Low)"
    case res128x160 = "128 x 160 (Nokia S40 v1)"
    case res176x208 = "176 x 208 (Nokia N-Gage / 6600)"
    case res176x220 = "176 x 220 (Sony Ericsson K700/K750i)"
    case res240x320 = "240 x 320 (Standard QVGA - Nokia N73/S40/S60)"
    case res320x240 = "320 x 240 (Landscape QVGA - E71)"
    case res240x400 = "240 x 400 (WQVGA Touch)"
    case res360x640 = "360 x 640 (Nokia 5800 XpressMusic)"
    case res480x800 = "480 x 800 (WVGA High-res)"
    case custom = "Custom"
    
    public var displayName: String {
        switch self {
        case .res128x128: return "128 x 128 (Vuông màn hình nhỏ)"
        case .res128x160: return "128 x 160 (Nokia S40 đời đầu)"
        case .res176x208: return "176 x 208 (Nokia 6600 / N-Gage)"
        case .res176x220: return "176 x 220 (Sony Ericsson K750i / K700)"
        case .res240x320: return "240 x 320 (Chuẩn QVGA - Nokia N73/S60)"
        case .res320x240: return "320 x 240 (Màn hình ngang - Nokia E71)"
        case .res240x400: return "240 x 400 (Màn hình dọc WQVGA)"
        case .res360x640: return "360 x 640 (Nokia 5800 Cảm ứng)"
        case .res480x800: return "480 x 800 (Độ nét cao WVGA)"
        case .custom: return "Tùy chỉnh độ phân giải..."
        }
    }
    
    public var dimensions: (width: Int, height: Int) {
        switch self {
        case .res128x128: return (128, 128)
        case .res128x160: return (128, 160)
        case .res176x208: return (176, 208)
        case .res176x220: return (176, 220)
        case .res240x320: return (240, 320)
        case .res320x240: return (320, 240)
        case .res240x400: return (240, 400)
        case .res360x640: return (360, 640)
        case .res480x800: return (480, 800)
        case .custom: return (240, 320)
        }
    }
}

public enum ScalingMode: String, Codable, CaseIterable {
    case fit = "Aspect Fit (Keep Ratio)"
    case stretch = "Stretch to Screen"
    case original = "1x Original Pixel"
    case fill = "Aspect Fill (Crop)"
    
    public var displayName: String {
        switch self {
        case .fit: return "Giữ nguyên tỉ lệ (Aspect Fit)"
        case .stretch: return "Kéo giãn toàn màn hình (Stretch)"
        case .original: return "Tỉ lệ pixel gốc 1:1"
        case .fill: return "Cắt lấp đầy màn hình (Crop Fill)"
        }
    }
}

public enum FilterMode: String, Codable, CaseIterable {
    case nearest = "Pixel Art (Nearest Neighbor)"
    case bilinear = "Smooth (Bilinear)"
    case crtScanlines = "Retro CRT Scanlines"
    case lcdGrid = "Nokia LCD Subpixel Grid"
    
    public var displayName: String {
        switch self {
        case .nearest: return "Pixel sắc nét (Nearest Neighbor)"
        case .bilinear: return "Làm mịn mượt (Bilinear)"
        case .crtScanlines: return "Quét sọc TV cổ điển (CRT Scanlines)"
        case .lcdGrid: return "Lưới điểm ảnh Nokia (LCD Grid)"
        }
    }
}

public enum KeypadLayout: String, Codable, CaseIterable {
    case classicPhone = "Retro Phone Keypad (Bottom)"
    case dpadBottom = "D-Pad + Action Buttons"
    case touchOverlay = "Transparent Touch Overlay"
    case splitLandscape = "Split Controls (Landscape)"
    case hidden = "Hide Keypad (Touchscreen Only)"
    
    public var displayName: String {
        switch self {
        case .classicPhone: return "Bàn phím điện thoại cổ điển (3x4 + D-Pad)"
        case .dpadBottom: return "Cụm phím D-Pad + Nút bấm"
        case .touchOverlay: return "Phím bấm mờ nổi trên màn hình"
        case .splitLandscape: return "Chia phím 2 bên khi xoay ngang"
        case .hidden: return "Ẩn bàn phím (Chơi cảm ứng thuần)"
        }
    }
}

public struct EmulatorConfig: Codable, Hashable {
    public var preset: ResolutionPreset
    public var customWidth: Int
    public var customHeight: Int
    public var targetFps: Int
    public var scalingMode: ScalingMode
    public var filterMode: FilterMode
    public var soundEnabled: Bool
    public var soundVolume: Float
    public var hapticFeedback: Bool
    public var keypadLayout: KeypadLayout
    public var keypadOpacity: Double
    public var touchScreenEnabled: Bool
    public var systemLocale: String
    public var backgroundKeepAlive: Bool
    public var networkKeepAlive: Bool
    public var vsyncMetal: Bool
    
    public init(
        preset: ResolutionPreset = .res240x320,
        customWidth: Int = 240,
        customHeight: Int = 320,
        targetFps: Int = 60,
        scalingMode: ScalingMode = .fit,
        filterMode: FilterMode = .nearest,
        soundEnabled: Bool = true,
        soundVolume: Float = 0.8,
        hapticFeedback: Bool = true,
        keypadLayout: KeypadLayout = .classicPhone,
        keypadOpacity: Double = 0.85,
        touchScreenEnabled: Bool = true,
        systemLocale: String = "vi-VN",
        backgroundKeepAlive: Bool = true,
        networkKeepAlive: Bool = true,
        vsyncMetal: Bool = true
    ) {
        self.preset = preset
        self.customWidth = customWidth
        self.customHeight = customHeight
        self.targetFps = targetFps
        self.scalingMode = scalingMode
        self.filterMode = filterMode
        self.soundEnabled = soundEnabled
        self.soundVolume = soundVolume
        self.hapticFeedback = hapticFeedback
        self.keypadLayout = keypadLayout
        self.keypadOpacity = keypadOpacity
        self.touchScreenEnabled = touchScreenEnabled
        self.systemLocale = systemLocale
        self.backgroundKeepAlive = backgroundKeepAlive
        self.networkKeepAlive = networkKeepAlive
        self.vsyncMetal = vsyncMetal
    }
    
    public var effectiveWidth: Int {
        return preset == .custom ? customWidth : preset.dimensions.width
    }
    
    public var effectiveHeight: Int {
        return preset == .custom ? customHeight : preset.dimensions.height
    }
}