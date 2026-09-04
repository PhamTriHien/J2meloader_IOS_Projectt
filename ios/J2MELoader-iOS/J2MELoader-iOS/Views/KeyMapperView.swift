import SwiftUI

public struct KeyMapperView: View {
    @State private var keyMappings: [String: Int32] = [
        "Phím Lên (DPad Up)": J2MEKey.up.rawValue,
        "Phím Xuống (DPad Down)": J2MEKey.down.rawValue,
        "Phím Trái (DPad Left)": J2MEKey.left.rawValue,
        "Phím Phải (DPad Right)": J2MEKey.right.rawValue,
        "Nút A / Nút Chọn (OK)": J2MEKey.fire.rawValue,
        "Nút B / Xóa (Clear)": J2MEKey.clear.rawValue,
        "Nút L1 / Phím mềm trái (LSK)": J2MEKey.softLeft.rawValue,
        "Nút R1 / Phím mềm phải (RSK)": J2MEKey.softRight.rawValue,
        "Nút Start / Gọi (Call)": J2MEKey.call.rawValue,
        "Nút Select / Kết thúc (End)": J2MEKey.end.rawValue,
        "Nút X (Phím số 5)": J2MEKey.num5.rawValue,
        "Nút Y (Phím số 7)": J2MEKey.num7.rawValue
    ]
    
    @Environment(\.presentationMode) var presentationMode
    
    public var body: some View {
        NavigationView {
            Form {
                Section(header: Text("Gán nút tay cầm Bluetooth / MFi")) {
                    Text("Kết nối tay cầm Bluetooth như Sony DualSense (PS5), Xbox Controller, Nintendo Switch hoặc tay cầm chuẩn MFi để chơi bằng phím cứng.")
                        .font(.footnote)
                        .foregroundColor(.secondary)
                    
                    ForEach(Array(keyMappings.keys.sorted()), id: \.self) { label in
                        HStack {
                            Text(label)
                            Spacer()
                            Picker("", selection: Binding(
                                get: { keyMappings[label] ?? 0 },
                                set: { keyMappings[label] = $0 }
                            )) {
                                ForEach(J2MEKey.allCases) { key in
                                    Text(key.displayName).tag(key.rawValue)
                                }
                            }
                            .labelsHidden()
                        }
                    }
                }
                
                Section {
                    Button("Khôi phục mặc định", role: .destructive) {
                        // Reset defaults
                    }
                }
            }
            .navigationTitle("Gán phím tay cầm")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Xong") {
                        presentationMode.wrappedValue.dismiss()
                    }
                    .font(.headline)
                    .foregroundColor(J2MEColors.accent)
                }
            }
        }
    }
}