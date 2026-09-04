import Foundation

public struct DeviceProfile: Identifiable, Codable, Hashable {
    public var id: String
    public var name: String
    public var platform: String
    public var width: Int
    public var height: Int
    public var keypadStyle: KeypadLayout
    public var soundEnabled: Bool
    public var defaultFps: Int
    public var systemProperties: [String: String]
    
    public static let defaultProfiles: [DeviceProfile] = [
        DeviceProfile(
            id: "nokia_n73",
            name: "Nokia N73 (Symbian S60v3)",
            platform: "NokiaN73-1/3.0638.0.0.1",
            width: 240,
            height: 320,
            keypadStyle: .classicPhone,
            soundEnabled: true,
            defaultFps: 60,
            systemProperties: [
                "microedition.platform": "NokiaN73",
                "microedition.profiles": "MIDP-2.0",
                "microedition.configuration": "CLDC-1.1",
                "microedition.m3g.version": "1.1"
            ]
        ),
        DeviceProfile(
            id: "se_k750i",
            name: "Sony Ericsson K750i",
            platform: "SonyEricssonK750i/R1CA021",
            width: 176,
            height: 220,
            keypadStyle: .classicPhone,
            soundEnabled: true,
            defaultFps: 60,
            systemProperties: [
                "microedition.platform": "SonyEricssonK750i",
                "microedition.profiles": "MIDP-2.0",
                "microedition.configuration": "CLDC-1.1"
            ]
        ),
        DeviceProfile(
            id: "nokia_6600",
            name: "Nokia 6600 (Classic S60v2)",
            platform: "Nokia6600/1.0",
            width: 176,
            height: 208,
            keypadStyle: .classicPhone,
            soundEnabled: true,
            defaultFps: 60,
            systemProperties: [
                "microedition.platform": "Nokia6600",
                "microedition.profiles": "MIDP-2.0",
                "microedition.configuration": "CLDC-1.0"
            ]
        ),
        DeviceProfile(
            id: "moto_v3",
            name: "Motorola RAZR V3",
            platform: "Motorola-V3",
            width: 176,
            height: 220,
            keypadStyle: .classicPhone,
            soundEnabled: true,
            defaultFps: 60,
            systemProperties: [
                "microedition.platform": "Motorola-V3",
                "microedition.profiles": "MIDP-2.0",
                "microedition.configuration": "CLDC-1.1"
            ]
        ),
        DeviceProfile(
            id: "nokia_5800",
            name: "Nokia 5800 XpressMusic (Touch)",
            platform: "Nokia5800XpressMusic",
            width: 360,
            height: 640,
            keypadStyle: .hidden,
            soundEnabled: true,
            defaultFps: 60,
            systemProperties: [
                "microedition.platform": "Nokia5800",
                "microedition.profiles": "MIDP-2.1",
                "microedition.configuration": "CLDC-1.1"
            ]
        )
    ]
}

public class ProfileManager: ObservableObject {
    public static let shared = ProfileManager()
    @Published public var profiles: [DeviceProfile] = DeviceProfile.defaultProfiles
}