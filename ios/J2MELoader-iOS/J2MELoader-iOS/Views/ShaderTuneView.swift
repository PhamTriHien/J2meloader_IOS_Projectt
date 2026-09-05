import SwiftUI

public struct ShaderTuneView: View {
    @State private var brightness: Double = 0.0
    @State private var contrast: Double = 1.0
    @State private var scanlineIntensity: Double = 0.25
    @State private var lcdGridStrength: Double = 0.20
    @State private var ghostingPersistence: Double = 0.10
    
    @Environment(\.presentationMode) var presentationMode
    
    public var body: some View {
        NavigationView {
            Form {
                Section(header: Text("TINH CHỈNH HIỆU ỨNG SHADER METAL").font(.system(size: 11.5, weight: .semibold))) {
                    VStack(alignment: .leading, spacing: 4) {
                        HStack {
                            Text("Độ sáng màn hình (Brightness)")
                                .font(.system(size: 13, weight: .regular))
                            Spacer()
                            Text(String(format: "%.2f", brightness))
                                .font(.system(size: 12, weight: .semibold, design: .monospaced))
                                .foregroundColor(.secondary)
                        }
                        Slider(value: $brightness, in: -0.5...0.5, step: 0.05)
                    }
                    
                    VStack(alignment: .leading, spacing: 4) {
                        HStack {
                            Text("Độ tương phản (Contrast)")
                                .font(.system(size: 13, weight: .regular))
                            Spacer()
                            Text(String(format: "%.2f", contrast))
                                .font(.system(size: 12, weight: .semibold, design: .monospaced))
                                .foregroundColor(.secondary)
                        }
                        Slider(value: $contrast, in: 0.5...2.0, step: 0.05)
                    }
                    
                    VStack(alignment: .leading, spacing: 4) {
                        HStack {
                            Text("Độ đậm quét sọc TV CRT (Scanlines)")
                                .font(.system(size: 13, weight: .regular))
                            Spacer()
                            Text("\(Int(scanlineIntensity * 100))%")
                                .font(.system(size: 12, weight: .semibold, design: .monospaced))
                                .foregroundColor(.secondary)
                        }
                        Slider(value: $scanlineIntensity, in: 0.0...1.0, step: 0.05)
                    }
                    
                    VStack(alignment: .leading, spacing: 4) {
                        HStack {
                            Text("Lưới điểm ảnh Nokia LCD (Grid Strength)")
                                .font(.system(size: 13, weight: .regular))
                            Spacer()
                            Text("\(Int(lcdGridStrength * 100))%")
                                .font(.system(size: 12, weight: .semibold, design: .monospaced))
                                .foregroundColor(.secondary)
                        }
                        Slider(value: $lcdGridStrength, in: 0.0...1.0, step: 0.05)
                    }
                    
                    VStack(alignment: .leading, spacing: 4) {
                        HStack {
                            Text("Độ bóng mờ bóng ma LCD (Ghosting)")
                                .font(.system(size: 13, weight: .regular))
                            Spacer()
                            Text("\(Int(ghostingPersistence * 100))%")
                                .font(.system(size: 12, weight: .semibold, design: .monospaced))
                                .foregroundColor(.secondary)
                        }
                        Slider(value: $ghostingPersistence, in: 0.0...0.5, step: 0.05)
                    }
                }
                
                Section {
                    Button("Khôi phục mặc định", role: .destructive) {
                        brightness = 0.0
                        contrast = 1.0
                        scanlineIntensity = 0.25
                        lcdGridStrength = 0.20
                        ghostingPersistence = 0.10
                    }
                    .font(.system(size: 13.5, weight: .regular))
                }
            }
            .navigationTitle("Hiệu ứng Shader")
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