import SwiftUI

public struct KeyMapperView: View {
    @State private var keyMappings: [String: Int32] = [
        "DPad Up": J2MEKey.up.rawValue,
        "DPad Down": J2MEKey.down.rawValue,
        "DPad Left": J2MEKey.left.rawValue,
        "DPad Right": J2MEKey.right.rawValue,
        "Action A / OK": J2MEKey.fire.rawValue,
        "Action B / Clear": J2MEKey.clear.rawValue,
        "Left Shoulder / LSK": J2MEKey.softLeft.rawValue,
        "Right Shoulder / RSK": J2MEKey.softRight.rawValue,
        "Start / Call": J2MEKey.call.rawValue,
        "Select / End": J2MEKey.end.rawValue,
        "Button X (Key 5)": J2MEKey.num5.rawValue,
        "Button Y (Key 7)": J2MEKey.num7.rawValue
    ]
    
    @Environment(\.presentationMode) var presentationMode
    
    public var body: some View {
        NavigationView {
            Form {
                Section(header: Text("Physical Game Controller & Remapping")) {
                    Text("Connect any Bluetooth MFi, Xbox, PlayStation DualSense, or Nintendo Switch controller to play with physical buttons.")
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
                    Button("Reset to Defaults", role: .destructive) {
                        // Reset defaults
                    }
                }
            }
            .navigationTitle("Key Mapper")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Done") {
                        presentationMode.wrappedValue.dismiss()
                    }
                    .font(.headline)
                }
            }
        }
    }
}