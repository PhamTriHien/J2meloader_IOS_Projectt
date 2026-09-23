import SwiftUI

public struct KeyMapperView: View {
    @ObservedObject private var gamePadManager = GamePadManager.shared
    @Environment(\.presentationMode) var presentationMode
    
    public init() {}
    
    public var body: some View {
        NavigationView {
            Form {
                Section(header: Text("TRẠNG THÁI TAY CẦM").font(.system(size: 11.5, weight: .semibold))) {
                    HStack(spacing: 10) {
                        Image(systemName: gamePadManager.isControllerConnected ? "gamecontroller.fill" : "gamecontroller")
                            .font(.system(size: 20))
                            .foregroundColor(gamePadManager.isControllerConnected ? .green : .secondary)
                        
                        VStack(alignment: .leading, spacing: 2) {
                            Text(gamePadManager.controllerName)
                                .font(.system(size: 13.5, weight: .semibold))
                                .foregroundColor(.primary)
                            
                            Text(gamePadManager.isControllerConnected ? "Sẵn sàng nhận tín hiệu điều khiển" : "Kết nối qua Bluetooth trong Cài đặt iOS")
                                .font(.system(size: 11.5))
                                .foregroundColor(.secondary)
                        }
                    }
                    .padding(.vertical, 3)
                }
                
                Section(header: Text("GÁN NÚT TAY CẦM BLUETOOTH / MFI").font(.system(size: 11.5, weight: .semibold))) {
                    ForEach(GamePadButton.allCases) { btn in
                        HStack {
                            Text(btn.rawValue)
                                .font(.system(size: 13, weight: .regular))
                            Spacer()
                            Picker("", selection: Binding(
                                get: { gamePadManager.keyForButton(btn) },
                                set: { newKeyVal in
                                    if let key = J2MEKey(rawValue: newKeyVal) {
                                        gamePadManager.updateMapping(button: btn, key: key)
                                    }
                                }
                            )) {
                                ForEach(J2MEKey.allCases) { key in
                                    Text(key.displayName)
                                        .font(.system(size: 12.5, weight: .regular))
                                        .tag(key.rawValue)
                                }
                            }
                            .labelsHidden()
                        }
                    }
                }
                
                Section {
                    Button("Khôi phục mặc định", role: .destructive) {
                        gamePadManager.resetToDefaults()
                    }
                    .font(.system(size: 13.5, weight: .regular))
                }
            }
            .navigationTitle("Gán phím tay cầm")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Xong") {
                        presentationMode.wrappedValue.dismiss()
                    }
                    .font(.system(size: 14, weight: .bold))
                    .foregroundColor(J2MEColors.accent)
                }
            }
        }
        .navigationViewStyle(StackNavigationViewStyle())
    }
}