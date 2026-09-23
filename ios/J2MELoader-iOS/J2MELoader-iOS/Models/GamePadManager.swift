import Foundation
import GameController

public enum GamePadButton: String, CaseIterable, Identifiable, Codable {
    case dpadUp = "Phím Lên (DPad Up)"
    case dpadDown = "Phím Xuống (DPad Down)"
    case dpadLeft = "Phím Trái (DPad Left)"
    case dpadRight = "Phím Phải (DPad Right)"
    case buttonA = "Nút A / Cross (OK)"
    case buttonB = "Nút B / Circle (Xóa)"
    case buttonX = "Nút X / Square (Số 5)"
    case buttonY = "Nút Y / Triangle (Số 7)"
    case leftShoulder = "Nút L1 / Phím mềm trái (LSK)"
    case rightShoulder = "Nút R1 / Phím mềm phải (RSK)"
    case buttonMenu = "Nút Start / Menu (Gọi)"
    case buttonOptions = "Nút Select / Options (Kết thúc)"
    
    public var id: String { rawValue }
    
    public var defaultKey: J2MEKey {
        switch self {
        case .dpadUp: return .up
        case .dpadDown: return .down
        case .dpadLeft: return .left
        case .dpadRight: return .right
        case .buttonA: return .fire
        case .buttonB: return .clear
        case .buttonX: return .num5
        case .buttonY: return .num7
        case .leftShoulder: return .softLeft
        case .rightShoulder: return .softRight
        case .buttonMenu: return .call
        case .buttonOptions: return .end
        }
    }
}

public class GamePadManager: ObservableObject {
    public static let shared = GamePadManager()
    
    @Published public var isControllerConnected: Bool = false
    @Published public var controllerName: String = "Chưa kết nối tay cầm"
    @Published public var mappings: [String: Int32] = [:]
    
    private let storageKey = "J2ME_GAMEPAD_MAPPINGS_V1"
    private var isMonitoring: Bool = false
    
    public init() {
        loadMappings()
        setupNotificationObservers()
        checkInitialControllers()
    }
    
    public func loadMappings() {
        if let saved = UserDefaults.standard.dictionary(forKey: storageKey) as? [String: Int32], !saved.isEmpty {
            self.mappings = saved
        } else {
            resetToDefaults()
        }
    }
    
    public func resetToDefaults() {
        var m: [String: Int32] = [:]
        for btn in GamePadButton.allCases {
            m[btn.rawValue] = btn.defaultKey.rawValue
        }
        self.mappings = m
        saveMappings()
    }
    
    public func saveMappings() {
        UserDefaults.standard.set(mappings, forKey: storageKey)
    }
    
    public func updateMapping(button: GamePadButton, key: J2MEKey) {
        mappings[button.rawValue] = key.rawValue
        saveMappings()
    }
    
    public func keyForButton(_ button: GamePadButton) -> Int32 {
        return mappings[button.rawValue] ?? button.defaultKey.rawValue
    }
    
    private func setupNotificationObservers() {
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(handleControllerConnected(_:)),
            name: .GCControllerDidConnect,
            object: nil
        )
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(handleControllerDisconnected(_:)),
            name: .GCControllerDidDisconnect,
            object: nil
        )
    }
    
    private func checkInitialControllers() {
        if let first = GCController.controllers().first {
            self.isControllerConnected = true
            self.controllerName = first.vendorName ?? "Tay cầm Game"
            bindController(first)
        } else {
            self.isControllerConnected = false
            self.controllerName = "Chưa kết nối tay cầm"
        }
    }
    
    @objc private func handleControllerConnected(_ notification: Notification) {
        DispatchQueue.main.async {
            if let controller = notification.object as? GCController {
                self.isControllerConnected = true
                self.controllerName = controller.vendorName ?? "Tay cầm Game"
                self.bindController(controller)
            }
        }
    }
    
    @objc private func handleControllerDisconnected(_ notification: Notification) {
        DispatchQueue.main.async {
            let remaining = GCController.controllers()
            if let next = remaining.first {
                self.isControllerConnected = true
                self.controllerName = next.vendorName ?? "Tay cầm Game"
                self.bindController(next)
            } else {
                self.isControllerConnected = false
                self.controllerName = "Chưa kết nối tay cầm"
            }
        }
    }
    
    public func startMonitoring() {
        isMonitoring = true
        for controller in GCController.controllers() {
            bindController(controller)
        }
    }
    
    public func stopMonitoring() {
        isMonitoring = false
    }
    
    private func dispatch(button: GamePadButton, isDown: Bool) {
        guard isMonitoring else { return }
        let keyCode = keyForButton(button)
        J2MEBridge.sendKeyEvent(keyCode, isDown: isDown)
    }
    
    private func bindController(_ controller: GCController) {
        guard let gamepad = controller.extendedGamepad else { return }
        
        // D-Pad
        gamepad.dpad.up.pressedChangedHandler = { [weak self] _, _, isPressed in
            self?.dispatch(button: .dpadUp, isDown: isPressed)
        }
        gamepad.dpad.down.pressedChangedHandler = { [weak self] _, _, isPressed in
            self?.dispatch(button: .dpadDown, isDown: isPressed)
        }
        gamepad.dpad.left.pressedChangedHandler = { [weak self] _, _, isPressed in
            self?.dispatch(button: .dpadLeft, isDown: isPressed)
        }
        gamepad.dpad.right.pressedChangedHandler = { [weak self] _, _, isPressed in
            self?.dispatch(button: .dpadRight, isDown: isPressed)
        }
        
        // Face Buttons
        gamepad.buttonA.pressedChangedHandler = { [weak self] _, _, isPressed in
            self?.dispatch(button: .buttonA, isDown: isPressed)
        }
        gamepad.buttonB.pressedChangedHandler = { [weak self] _, _, isPressed in
            self?.dispatch(button: .buttonB, isDown: isPressed)
        }
        gamepad.buttonX.pressedChangedHandler = { [weak self] _, _, isPressed in
            self?.dispatch(button: .buttonX, isDown: isPressed)
        }
        gamepad.buttonY.pressedChangedHandler = { [weak self] _, _, isPressed in
            self?.dispatch(button: .buttonY, isDown: isPressed)
        }
        
        // Shoulders
        gamepad.leftShoulder.pressedChangedHandler = { [weak self] _, _, isPressed in
            self?.dispatch(button: .leftShoulder, isDown: isPressed)
        }
        gamepad.rightShoulder.pressedChangedHandler = { [weak self] _, _, isPressed in
            self?.dispatch(button: .rightShoulder, isDown: isPressed)
        }
        
        // Menu / Options
        gamepad.buttonMenu.pressedChangedHandler = { [weak self] _, _, isPressed in
            self?.dispatch(button: .buttonMenu, isDown: isPressed)
        }
        gamepad.buttonOptions?.pressedChangedHandler = { [weak self] _, _, isPressed in
            self?.dispatch(button: .buttonOptions, isDown: isPressed)
        }
        
        // Thumbstick threshold emulation for D-Pad
        var stickUp = false, stickDown = false, stickLeft = false, stickRight = false
        gamepad.leftThumbstick.valueChangedHandler = { [weak self] _, x, y in
            guard let self = self else { return }
            let threshold: Float = 0.5
            
            let up = y > threshold
            if up != stickUp {
                stickUp = up
                self.dispatch(button: .dpadUp, isDown: up)
            }
            let down = y < -threshold
            if down != stickDown {
                stickDown = down
                self.dispatch(button: .dpadDown, isDown: down)
            }
            let left = x < -threshold
            if left != stickLeft {
                stickLeft = left
                self.dispatch(button: .dpadLeft, isDown: left)
            }
            let right = x > threshold
            if right != stickRight {
                stickRight = right
                self.dispatch(button: .dpadRight, isDown: right)
            }
        }
    }
}
