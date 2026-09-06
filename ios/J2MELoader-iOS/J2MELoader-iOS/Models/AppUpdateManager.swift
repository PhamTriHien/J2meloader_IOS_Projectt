import Foundation
import SwiftUI

public struct ReleaseInfo: Codable, Identifiable {
    public var id: Int
    public var tagName: String
    public var name: String
    public var body: String
    public var publishedAt: String
    public var ipaDownloadUrl: String?
    public var ipaSize: Int?
    
    enum CodingKeys: String, CodingKey {
        case id
        case tagName = "tag_name"
        case name
        case body
        case publishedAt = "published_at"
        case assets
    }
    
    public init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        id = try container.decode(Int.self, forKey: .id)
        tagName = try container.decode(String.self, forKey: .tagName)
        name = try container.decodeIfPresent(String.self, forKey: .name) ?? tagName
        body = try container.decodeIfPresent(String.self, forKey: .body) ?? ""
        publishedAt = try container.decodeIfPresent(String.self, forKey: .publishedAt) ?? ""
        
        if let assets = try? container.decode([ReleaseAsset].self, forKey: .assets) {
            if let ipaAsset = assets.first(where: { $0.name.hasSuffix(".ipa") }) {
                ipaDownloadUrl = ipaAsset.browserDownloadUrl
                ipaSize = ipaAsset.size
            }
        }
    }
}

public struct ReleaseAsset: Codable {
    public var name: String
    public var browserDownloadUrl: String
    public var size: Int
    
    enum CodingKeys: String, CodingKey {
        case name
        case browserDownloadUrl = "browser_download_url"
        case size
    }
}

public class AppUpdateManager: NSObject, ObservableObject, URLSessionDownloadDelegate {
    public static let shared = AppUpdateManager()
    
    // Current application version string
    public static let currentVersion = "v1.8.2-j2hienloader"
    public static let currentBuildTag = "1.8.2"
    
    private let repo = "PhamTriHien/J2meloader_IOS_Projectt"
    
    @Published public var isChecking: Bool = false
    @Published public var hasUpdate: Bool = false
    @Published public var latestRelease: ReleaseInfo? = nil
    @Published public var statusMessage: String? = nil
    @Published public var isDownloading: Bool = false
    @Published public var downloadProgress: Double = 0.0
    @Published public var downloadedIpaUrl: URL? = nil
    @Published public var showingUpdateModal: Bool = false
    @Published public var downloadError: String? = nil
    
    @AppStorage("autoCheckUpdates") public var autoCheckUpdates: Bool = true
    @AppStorage("lastDismissedVersion") public var lastDismissedVersion: String = ""
    
    private var downloadTask: URLSessionDownloadTask?
    
    public override init() {
        super.init()
    }
    
    public func checkForUpdates(manual: Bool = false) {
        guard !isChecking else { return }
        if !manual && !autoCheckUpdates { return }
        
        isChecking = true
        statusMessage = manual ? "Đang kiểm tra bản cập nhật..." : nil
        
        guard let url = URL(string: "https://api.github.com/repos/\(repo)/releases/latest") else {
            isChecking = false
            return
        }
        
        var request = URLRequest(url: url)
        request.setValue("Mozilla/5.0", forHTTPHeaderField: "User-Agent")
        request.setValue("application/vnd.github.v3+json", forHTTPHeaderField: "Accept")
        request.cachePolicy = .reloadIgnoringLocalCacheData
        request.timeoutInterval = 12
        
        URLSession.shared.dataTask(with: request) { [weak self] data, response, error in
            DispatchQueue.main.async {
                guard let self = self else { return }
                self.isChecking = false
                
                if let error = error {
                    if manual {
                        self.statusMessage = "Không có kết nối mạng (\(error.localizedDescription))"
                    }
                    return
                }
                
                guard let data = data else {
                    if manual { self.statusMessage = "Không nhận được phản hồi từ máy chủ." }
                    return
                }
                
                do {
                    let release = try JSONDecoder().decode(ReleaseInfo.self, from: data)
                    self.latestRelease = release
                    
                    let isNewer = self.isVersionNewer(remoteTag: release.tagName, currentTag: AppUpdateManager.currentVersion)
                    self.hasUpdate = isNewer
                    
                    if isNewer {
                        self.statusMessage = "Có bản cập nhật mới: \(release.tagName)"
                        if manual || self.lastDismissedVersion != release.tagName {
                            self.showingUpdateModal = true
                        }
                    } else {
                        if manual {
                            self.statusMessage = "Ứng dụng đang ở phiên bản mới nhất (\(AppUpdateManager.currentVersion))"
                        }
                    }
                } catch {
                    if manual {
                        self.statusMessage = "Lỗi phân tích bản phát hành: \(error.localizedDescription)"
                    }
                }
            }
        }.resume()
    }
    
    private func isVersionNewer(remoteTag: String, currentTag: String) -> Bool {
        if remoteTag == currentTag { return false }
        
        // Extract numeric versions e.g. "v1.8.3" -> "1.8.3"
        let rClean = remoteTag.replacingOccurrences(of: "v", with: "").components(separatedBy: "-").first ?? ""
        let cClean = currentTag.replacingOccurrences(of: "v", with: "").components(separatedBy: "-").first ?? ""
        
        let rParts = rClean.components(separatedBy: ".").compactMap { Int($0) }
        let cParts = cClean.components(separatedBy: ".").compactMap { Int($0) }
        
        for i in 0..<max(rParts.count, cParts.count) {
            let rVal = i < rParts.count ? rParts[i] : 0
            let cVal = i < cParts.count ? cParts[i] : 0
            if rVal > cVal { return true }
            if rVal < cVal { return false }
        }
        
        return false
    }
    
    public func startDownloadingIpa() {
        guard let downloadUrlStr = latestRelease?.ipaDownloadUrl,
              let downloadUrl = URL(string: downloadUrlStr) else {
            self.downloadError = "Không tìm thấy link tải IPA trực tiếp."
            return
        }
        
        isDownloading = true
        downloadProgress = 0.0
        downloadError = nil
        downloadedIpaUrl = nil
        
        let config = URLSessionConfiguration.default
        let session = URLSession(configuration: config, delegate: self, delegateQueue: nil)
        downloadTask = session.downloadTask(with: downloadUrl)
        downloadTask?.resume()
    }
    
    public func cancelDownload() {
        downloadTask?.cancel()
        isDownloading = false
        downloadProgress = 0.0
    }
    
    public func dismissUpdate() {
        if let currentTag = latestRelease?.tagName {
            lastDismissedVersion = currentTag
        }
        showingUpdateModal = false
    }
    
    // MARK: - URLSessionDownloadDelegate
    public func urlSession(_ session: URLSession, downloadTask: URLSessionDownloadTask, didFinishDownloadingTo location: URL) {
        do {
            let fileManager = FileManager.default
            let docsUrl = fileManager.urls(for: .documentDirectory, in: .userDomainMask)[0]
            let destUrl = docsUrl.appendingPathComponent("J2HienLoader_Update.ipa")
            
            if fileManager.fileExists(atPath: destUrl.path) {
                try fileManager.removeItem(at: destUrl)
            }
            try fileManager.moveItem(at: location, to: destUrl)
            
            DispatchQueue.main.async {
                self.isDownloading = false
                self.downloadProgress = 1.0
                self.downloadedIpaUrl = destUrl
            }
        } catch {
            DispatchQueue.main.async {
                self.isDownloading = false
                self.downloadError = "Lỗi lưu file: \(error.localizedDescription)"
            }
        }
    }
    
    public func urlSession(_ session: URLSession, downloadTask: URLSessionDownloadTask, didWriteData bytesWritten: Int64, totalBytesWritten: Int64, totalBytesExpectedToWrite: Int64) {
        if totalBytesExpectedToWrite > 0 {
            let progress = Double(totalBytesWritten) / Double(totalBytesExpectedToWrite)
            DispatchQueue.main.async {
                self.downloadProgress = progress
            }
        }
    }
}
