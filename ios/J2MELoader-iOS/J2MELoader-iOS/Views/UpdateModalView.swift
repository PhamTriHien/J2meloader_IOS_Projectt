import SwiftUI

public struct UpdateModalView: View {
    @ObservedObject var updateManager = AppUpdateManager.shared
    @Environment(\.presentationMode) var presentationMode
    @State private var showingShareSheet = false
    
    public init() {}
    
    public var body: some View {
        NavigationView {
            ZStack {
                Color(.systemGroupedBackground).ignoresSafeArea()
                
                VStack(spacing: 0) {
                    ScrollView {
                        VStack(spacing: 16) {
                            // Header Icon & Version Badge
                            VStack(spacing: 8) {
                                ZStack {
                                    Circle()
                                        .fill(LinearGradient(gradient: Gradient(colors: [J2MEColors.accent, Color.orange]), startPoint: .topLeading, endPoint: .bottomTrailing))
                                        .frame(width: 72, height: 72)
                                        .shadow(color: J2MEColors.accent.opacity(0.3), radius: 10, x: 0, y: 5)
                                    
                                    Image(systemName: "arrow.down.circle.fill")
                                        .font(.system(size: 36, weight: .bold))
                                        .foregroundColor(.white)
                                }
                                .padding(.top, 8)
                                
                                Text(updateManager.latestRelease?.name ?? "Bản cập nhật mới")
                                    .font(.system(size: 18, weight: .bold))
                                    .multilineTextAlignment(.center)
                                    .foregroundColor(.primary)
                                
                                HStack(spacing: 6) {
                                    Text("Phiên bản:")
                                        .font(.system(size: 12, weight: .regular))
                                        .foregroundColor(.secondary)
                                    Text(updateManager.latestRelease?.tagName ?? "")
                                        .font(.system(size: 12, weight: .bold, design: .monospaced))
                                        .padding(.horizontal, 6)
                                        .padding(.vertical, 2)
                                        .background(J2MEColors.accent.opacity(0.15))
                                        .foregroundColor(J2MEColors.accent)
                                        .cornerRadius(4)
                                }
                            }
                            .padding(.vertical, 8)
                            
                            // Release Notes Box
                            VStack(alignment: .leading, spacing: 8) {
                                Label("Nội dung bản vá mới", systemImage: "sparkles")
                                    .font(.system(size: 13, weight: .bold))
                                    .foregroundColor(.primary)
                                
                                Text(updateManager.latestRelease?.body.isEmpty == false ? (updateManager.latestRelease?.body ?? "") : "Bản vá tối ưu hóa hiệu năng, sửa lỗi tải game và nâng cấp giả lập J2ME.")
                                    .font(.system(size: 12.5, weight: .regular))
                                    .foregroundColor(.secondary)
                                    .frame(maxWidth: .infinity, alignment: .leading)
                                    .padding(10)
                                    .background(Color(.secondarySystemGroupedBackground))
                                    .cornerRadius(8)
                            }
                            .padding(.horizontal, 16)
                            
                            // Download Progress (if downloading)
                            if updateManager.isDownloading {
                                VStack(spacing: 8) {
                                    HStack {
                                        Text("Đang tải bản cập nhật...")
                                            .font(.system(size: 12, weight: .medium))
                                        Spacer()
                                        Text("\(Int(updateManager.downloadProgress * 100))%")
                                            .font(.system(size: 12, weight: .bold, design: .monospaced))
                                    }
                                    
                                    ProgressView(value: updateManager.downloadProgress)
                                        .accentColor(J2MEColors.accent)
                                }
                                .padding(12)
                                .background(Color(.secondarySystemGroupedBackground))
                                .cornerRadius(8)
                                .padding(.horizontal, 16)
                            }
                            
                            if let error = updateManager.downloadError {
                                Text(error)
                                    .font(.system(size: 12, weight: .medium))
                                    .foregroundColor(.red)
                                    .padding(.horizontal, 16)
                            }
                        }
                        .padding(.vertical, 8)
                    }
                    
                    // Action Buttons Footer
                    VStack(spacing: 10) {
                        if updateManager.downloadedIpaUrl != nil {
                            // Download completed -> Share to ESign / TrollStore
                            Button(action: {
                                showingShareSheet = true
                            }) {
                                HStack(spacing: 6) {
                                    Image(systemName: "square.and.arrow.up.fill")
                                        .font(.system(size: 14, weight: .bold))
                                    Text("CÀI ĐẶT QUA ESIGN / TROLLSTORE")
                                        .font(.system(size: 14, weight: .bold))
                                }
                                .foregroundColor(.white)
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 12)
                                .background(Color.green)
                                .cornerRadius(10)
                            }
                        } else if !updateManager.isDownloading {
                            // Start In-App Download Button
                            Button(action: {
                                updateManager.startDownloadingIpa()
                            }) {
                                HStack(spacing: 6) {
                                    Image(systemName: "arrow.down.circle.fill")
                                        .font(.system(size: 14, weight: .bold))
                                    Text("TỰ ĐỘNG TẢI BẢN VÁ NGAY")
                                        .font(.system(size: 14, weight: .bold))
                                }
                                .foregroundColor(.white)
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 12)
                                .background(J2MEColors.accent)
                                .cornerRadius(10)
                            }
                            
                            // Open Safari Direct Link Button
                            if let dlUrlStr = updateManager.latestRelease?.ipaDownloadUrl,
                               let dlUrl = URL(string: dlUrlStr) {
                                Link(destination: dlUrl) {
                                    HStack(spacing: 6) {
                                        Image(systemName: "safari.fill")
                                            .font(.system(size: 13, weight: .semibold))
                                        Text("Tải qua Safari (Link Trực tiếp)")
                                            .font(.system(size: 13, weight: .semibold))
                                    }
                                    .foregroundColor(.primary)
                                    .frame(maxWidth: .infinity)
                                    .padding(.vertical, 10)
                                    .background(Color(.secondarySystemBackground))
                                    .cornerRadius(10)
                                }
                            }
                        }
                        
                        Button(action: {
                            updateManager.dismissUpdate()
                            presentationMode.wrappedValue.dismiss()
                        }) {
                            Text("Để sau")
                                .font(.system(size: 13, weight: .medium))
                                .foregroundColor(.secondary)
                        }
                        .padding(.top, 2)
                    }
                    .padding(16)
                    .background(Color(.systemBackground))
                    .shadow(color: Color.black.opacity(0.05), radius: 5, x: 0, y: -2)
                }
            }
            .navigationTitle("Cập nhật ứng dụng")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button(action: {
                        updateManager.dismissUpdate()
                        presentationMode.wrappedValue.dismiss()
                    }) {
                        Image(systemName: "xmark.circle.fill")
                            .font(.system(size: 16))
                            .foregroundColor(.secondary)
                    }
                }
            }
            .sheet(isPresented: $showingShareSheet) {
                if let fileUrl = updateManager.downloadedIpaUrl {
                    ActivityView(activityItems: [fileUrl])
                }
            }
        }
        .navigationViewStyle(StackNavigationViewStyle())
    }
}

// MARK: - UIActivityViewController Wrapper for SwiftUI
struct ActivityView: UIViewControllerRepresentable {
    let activityItems: [Any]
    let applicationActivities: [UIActivity]? = nil
    
    func makeUIViewController(context: Context) -> UIActivityViewController {
        let controller = UIActivityViewController(activityItems: activityItems, applicationActivities: applicationActivities)
        return controller
    }
    
    func updateUIViewController(_ uiViewController: UIActivityViewController, context: Context) {}
}
