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
                Section(header: Text("Post-Processing Shader Tuning")) {
                    VStack(alignment: .leading, spacing: 6) {
                        HStack {
                            Text("Brightness")
                            Spacer()
                            Text(String(format: "%.2f", brightness))
                                .foregroundColor(.secondary)
                        }
                        Slider(value: $brightness, in: -0.5...0.5, step: 0.05)
                    }
                    
                    VStack(alignment: .leading, spacing: 6) {
                        HStack {
                            Text("Contrast")
                            Spacer()
                            Text(String(format: "%.2f", contrast))
                                .foregroundColor(.secondary)
                        }
                        Slider(value: $contrast, in: 0.5...2.0, step: 0.05)
                    }
                    
                    VStack(alignment: .leading, spacing: 6) {
                        HStack {
                            Text("CRT Scanlines Intensity")
                            Spacer()
                            Text("\(Int(scanlineIntensity * 100))%")
                                .foregroundColor(.secondary)
                        }
                        Slider(value: $scanlineIntensity, in: 0.0...1.0, step: 0.05)
                    }
                    
                    VStack(alignment: .leading, spacing: 6) {
                        HStack {
                            Text("LCD Subpixel Grid")
                            Spacer()
                            Text("\(Int(lcdGridStrength * 100))%")
                                .foregroundColor(.secondary)
                        }
                        Slider(value: $lcdGridStrength, in: 0.0...1.0, step: 0.05)
                    }
                    
                    VStack(alignment: .leading, spacing: 6) {
                        HStack {
                            Text("LCD Ghosting / Persistence")
                            Spacer()
                            Text("\(Int(ghostingPersistence * 100))%")
                                .foregroundColor(.secondary)
                        }
                        Slider(value: $ghostingPersistence, in: 0.0...0.5, step: 0.05)
                    }
                }
                
                Section {
                    Button("Reset Shader Settings", role: .destructive) {
                        brightness = 0.0
                        contrast = 1.0
                        scanlineIntensity = 0.25
                        lcdGridStrength = 0.20
                        ghostingPersistence = 0.10
                    }
                }
            }
            .navigationTitle("Shader Tune")
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