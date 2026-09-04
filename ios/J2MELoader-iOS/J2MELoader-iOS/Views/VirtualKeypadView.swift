import SwiftUI

public struct VirtualKeypadView: View {
    public var config: EmulatorConfig
    public var onKeyPress: (Int32, Bool) -> Void
    
    public init(config: EmulatorConfig, onKeyPress: @escaping (Int32, Bool) -> Void) {
        self.config = config
        self.onKeyPress = onKeyPress
    }
    
    public var body: some View {
        Group {
            switch config.keypadLayout {
            case .classicPhone:
                ClassicPhoneKeypad(config: config, onKeyPress: onKeyPress)
            case .dpadBottom:
                GamepadDpadLayout(config: config, onKeyPress: onKeyPress)
            case .splitLandscape:
                SplitLandscapeLayout(config: config, onKeyPress: onKeyPress)
            case .touchOverlay:
                ClassicPhoneKeypad(config: config, onKeyPress: onKeyPress)
            case .hidden:
                EmptyView()
            }
        }
    }
}

// MARK: - Classic Phone Keypad (3x4 + D-Pad + LSK/RSK)
struct ClassicPhoneKeypad: View {
    let config: EmulatorConfig
    let onKeyPress: (Int32, Bool) -> Void
    
    var body: some View {
        VStack(spacing: 6) {
            // Function & Soft keys + D-Pad row
            HStack(spacing: 12) {
                // Left Column: Soft 1 & Call
                VStack(spacing: 6) {
                    KeyButton(title: "LSK", key: .softLeft, color: Color(.systemGray4), haptic: config.hapticFeedback, onEvent: onKeyPress)
                        .frame(width: 56, height: 38)
                    
                    KeyButton(title: "✆", key: .call, color: .green.opacity(0.85), haptic: config.hapticFeedback, onEvent: onKeyPress)
                        .frame(width: 56, height: 38)
                }
                
                // Center Column: D-Pad
                ZStack {
                    RoundedRectangle(cornerRadius: 16)
                        .fill(Color(.tertiarySystemFill))
                        .frame(width: 140, height: 96)
                    
                    VStack(spacing: 2) {
                        KeyButton(title: "▲", key: .up, haptic: config.hapticFeedback, onEvent: onKeyPress)
                            .frame(width: 44, height: 26)
                        
                        HStack(spacing: 14) {
                            KeyButton(title: "◀", key: .left, haptic: config.hapticFeedback, onEvent: onKeyPress)
                                .frame(width: 36, height: 32)
                            
                            KeyButton(title: "OK", key: .fire, color: .accentColor, haptic: config.hapticFeedback, onEvent: onKeyPress)
                                .frame(width: 44, height: 32)
                            
                            KeyButton(title: "▶", key: .right, haptic: config.hapticFeedback, onEvent: onKeyPress)
                                .frame(width: 36, height: 32)
                        }
                        
                        KeyButton(title: "▼", key: .down, haptic: config.hapticFeedback, onEvent: onKeyPress)
                            .frame(width: 44, height: 26)
                    }
                }
                
                // Right Column: Soft 2 & End
                VStack(spacing: 6) {
                    KeyButton(title: "RSK", key: .softRight, color: Color(.systemGray4), haptic: config.hapticFeedback, onEvent: onKeyPress)
                        .frame(width: 56, height: 38)
                    
                    KeyButton(title: "✖", key: .end, color: .red.opacity(0.85), haptic: config.hapticFeedback, onEvent: onKeyPress)
                        .frame(width: 56, height: 38)
                }
            }
            .padding(.horizontal, 8)
            
            // Numeric 3x4 Keypad (1-9, *, 0, #)
            VStack(spacing: 5) {
                HStack(spacing: 8) {
                    KeyButton(title: "1", sub: ".,-", key: .num1, haptic: config.hapticFeedback, onEvent: onKeyPress)
                    KeyButton(title: "2", sub: "abc", key: .num2, haptic: config.hapticFeedback, onEvent: onKeyPress)
                    KeyButton(title: "3", sub: "def", key: .num3, haptic: config.hapticFeedback, onEvent: onKeyPress)
                }
                HStack(spacing: 8) {
                    KeyButton(title: "4", sub: "ghi", key: .num4, haptic: config.hapticFeedback, onEvent: onKeyPress)
                    KeyButton(title: "5", sub: "jkl", key: .num5, haptic: config.hapticFeedback, onEvent: onKeyPress)
                    KeyButton(title: "6", sub: "mno", key: .num6, haptic: config.hapticFeedback, onEvent: onKeyPress)
                }
                HStack(spacing: 8) {
                    KeyButton(title: "7", sub: "pqrs", key: .num7, haptic: config.hapticFeedback, onEvent: onKeyPress)
                    KeyButton(title: "8", sub: "tuv", key: .num8, haptic: config.hapticFeedback, onEvent: onKeyPress)
                    KeyButton(title: "9", sub: "wxyz", key: .num9, haptic: config.hapticFeedback, onEvent: onKeyPress)
                }
                HStack(spacing: 8) {
                    KeyButton(title: "*", sub: "␣", key: .star, haptic: config.hapticFeedback, onEvent: onKeyPress)
                    KeyButton(title: "0", sub: "+", key: .num0, haptic: config.hapticFeedback, onEvent: onKeyPress)
                    KeyButton(title: "#", sub: "⇧", key: .pound, haptic: config.hapticFeedback, onEvent: onKeyPress)
                }
            }
            .padding(.horizontal, 16)
        }
        .padding(.vertical, 8)
        .background(
            RoundedRectangle(cornerRadius: 22)
                .fill(Color(.secondarySystemBackground).opacity(config.keypadOpacity))
                .shadow(color: Color.black.opacity(0.12), radius: 8, x: 0, y: -2)
        )
        .padding(.horizontal, 8)
    }
}

// MARK: - Gamepad D-Pad + Action Buttons Layout
struct GamepadDpadLayout: View {
    let config: EmulatorConfig
    let onKeyPress: (Int32, Bool) -> Void
    
    var body: some View {
        HStack(spacing: 24) {
            // Left: D-Pad
            VStack(spacing: 4) {
                KeyButton(title: "▲", key: .up, haptic: config.hapticFeedback, onEvent: onKeyPress)
                    .frame(width: 48, height: 40)
                
                HStack(spacing: 12) {
                    KeyButton(title: "◀", key: .left, haptic: config.hapticFeedback, onEvent: onKeyPress)
                        .frame(width: 48, height: 40)
                    
                    KeyButton(title: "LSK", key: .softLeft, color: Color(.systemGray4), haptic: config.hapticFeedback, onEvent: onKeyPress)
                        .frame(width: 44, height: 40)
                    
                    KeyButton(title: "▶", key: .right, haptic: config.hapticFeedback, onEvent: onKeyPress)
                        .frame(width: 48, height: 40)
                }
                
                KeyButton(title: "▼", key: .down, haptic: config.hapticFeedback, onEvent: onKeyPress)
                    .frame(width: 48, height: 40)
            }
            
            Spacer()
            
            // Right: Diamond Action Buttons (Fire/5, 7, 9, RSK)
            VStack(spacing: 4) {
                KeyButton(title: "5", key: .num5, color: .accentColor, haptic: config.hapticFeedback, onEvent: onKeyPress)
                    .frame(width: 48, height: 40)
                
                HStack(spacing: 12) {
                    KeyButton(title: "7", key: .num7, haptic: config.hapticFeedback, onEvent: onKeyPress)
                        .frame(width: 48, height: 40)
                    
                    KeyButton(title: "OK", key: .fire, color: .accentColor, haptic: config.hapticFeedback, onEvent: onKeyPress)
                        .frame(width: 44, height: 40)
                    
                    KeyButton(title: "9", key: .num9, haptic: config.hapticFeedback, onEvent: onKeyPress)
                        .frame(width: 48, height: 40)
                }
                
                KeyButton(title: "RSK", key: .softRight, color: Color(.systemGray4), haptic: config.hapticFeedback, onEvent: onKeyPress)
                    .frame(width: 48, height: 40)
            }
        }
        .padding(16)
        .background(
            RoundedRectangle(cornerRadius: 22)
                .fill(Color(.secondarySystemBackground).opacity(config.keypadOpacity))
        )
        .padding(.horizontal, 12)
    }
}

// MARK: - Split Landscape Layout
struct SplitLandscapeLayout: View {
    let config: EmulatorConfig
    let onKeyPress: (Int32, Bool) -> Void
    
    var body: some View {
        HStack {
            // Left D-Pad
            VStack(spacing: 4) {
                KeyButton(title: "▲", key: .up, haptic: config.hapticFeedback, onEvent: onKeyPress).frame(width: 44, height: 36)
                HStack(spacing: 12) {
                    KeyButton(title: "◀", key: .left, haptic: config.hapticFeedback, onEvent: onKeyPress).frame(width: 44, height: 36)
                    KeyButton(title: "LSK", key: .softLeft, haptic: config.hapticFeedback, onEvent: onKeyPress).frame(width: 44, height: 36)
                    KeyButton(title: "▶", key: .right, haptic: config.hapticFeedback, onEvent: onKeyPress).frame(width: 44, height: 36)
                }
                KeyButton(title: "▼", key: .down, haptic: config.hapticFeedback, onEvent: onKeyPress).frame(width: 44, height: 36)
            }
            
            Spacer()
            
            // Right Action Buttons
            VStack(spacing: 4) {
                KeyButton(title: "OK", key: .fire, color: .accentColor, haptic: config.hapticFeedback, onEvent: onKeyPress).frame(width: 44, height: 36)
                HStack(spacing: 12) {
                    KeyButton(title: "5", key: .num5, color: .accentColor, haptic: config.hapticFeedback, onEvent: onKeyPress).frame(width: 44, height: 36)
                    KeyButton(title: "RSK", key: .softRight, haptic: config.hapticFeedback, onEvent: onKeyPress).frame(width: 44, height: 36)
                    KeyButton(title: "0", key: .num0, haptic: config.hapticFeedback, onEvent: onKeyPress).frame(width: 44, height: 36)
                }
                KeyButton(title: "CLR", key: .clear, color: .red.opacity(0.8), haptic: config.hapticFeedback, onEvent: onKeyPress).frame(width: 44, height: 36)
            }
        }
        .padding(.horizontal, 20)
    }
}

// MARK: - KeyButton Component
struct KeyButton: View {
    let title: String
    var sub: String?
    let key: J2MEKey
    var color: Color
    let haptic: Bool
    let onEvent: (Int32, Bool) -> Void
    
    @State private var isPressed: Bool = false
    private let generator = UIImpactFeedbackGenerator(style: .medium)
    
    init(title: String, sub: String? = nil, key: J2MEKey, color: Color = Color(.systemGray5), haptic: Bool, onEvent: @escaping (Int32, Bool) -> Void) {
        self.title = title
        self.sub = sub
        self.key = key
        self.color = color
        self.haptic = haptic
        self.onEvent = onEvent
    }
    
    var body: some View {
        GeometryReader { geo in
            ZStack {
                RoundedRectangle(cornerRadius: 10)
                    .fill(isPressed ? Color.accentColor : color)
                    .overlay(
                        RoundedRectangle(cornerRadius: 10)
                            .stroke(Color.white.opacity(0.15), lineWidth: 1)
                    )
                    .shadow(color: Color.black.opacity(isPressed ? 0.0 : 0.08), radius: 2, x: 0, y: 1)
                
                VStack(spacing: 1) {
                    Text(title)
                        .font(.system(size: sub != nil ? 17 : 15, weight: .bold, design: .rounded))
                        .foregroundColor(isPressed ? .white : .primary)
                    
                    if let sub = sub {
                        Text(sub)
                            .font(.system(size: 8, weight: .semibold))
                            .foregroundColor(isPressed ? .white.opacity(0.8) : .secondary)
                    }
                }
            }
            .scaleEffect(isPressed ? 0.94 : 1.0)
            .animation(.easeInOut(duration: 0.08), value: isPressed)
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { _ in
                        if !isPressed {
                            isPressed = true
                            if haptic { generator.impactOccurred() }
                            onEvent(key.rawValue, true)
                        }
                    }
                    .onEnded { _ in
                        isPressed = false
                        onEvent(key.rawValue, false)
                    }
            )
        }
        .frame(height: sub != nil ? 40 : nil)
    }
}