import Foundation

public enum J2MEKey: Int32, CaseIterable, Identifiable {
    case num0 = 48
    case num1 = 49
    case num2 = 50
    case num3 = 51
    case num4 = 52
    case num5 = 53
    case num6 = 54
    case num7 = 55
    case num8 = 56
    case num9 = 57
    case star = 42
    case pound = 35
    
    // Direction & Actions (standard Canvas negative keycodes)
    case up = -1
    case down = -2
    case left = -3
    case right = -4
    case fire = -5
    case softLeft = -6
    case softRight = -7
    case clear = -8
    case call = -10
    case end = -11
    
    public var id: Int32 { rawValue }
    
    public var displayName: String {
        switch self {
        case .num0: return "0"
        case .num1: return "1"
        case .num2: return "2"
        case .num3: return "3"
        case .num4: return "4"
        case .num5: return "5"
        case .num6: return "6"
        case .num7: return "7"
        case .num8: return "8"
        case .num9: return "9"
        case .star: return "*"
        case .pound: return "#"
        case .up: return "▲"
        case .down: return "▼"
        case .left: return "◀"
        case .right: return "▶"
        case .fire: return "OK"
        case .softLeft: return "LSK"
        case .softRight: return "RSK"
        case .clear: return "CLR"
        case .call: return "✆"
        case .end: return "✖"
        }
    }
}