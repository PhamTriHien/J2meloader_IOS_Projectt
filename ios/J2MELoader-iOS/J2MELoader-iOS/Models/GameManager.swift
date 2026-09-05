import Foundation
import SwiftUI
import Combine

public class GameManager: ObservableObject {
    @Published public var games: [GameItem] = []
    @Published public var currentGame: GameItem?
    @Published public var isEmulating: Bool = false
    @Published public var isImporting: Bool = false
    @Published public var errorMessage: String?
    @Published public var showErrorAlert: Bool = false
    
    private let gamesStorageKey = "J2ME_INSTALLED_GAMES_V1"
    
    public init() {
        loadGames()
    }
    
    public var documentsDirectory: URL {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
    }
    
    public var gamesDirectory: URL {
        let dir = documentsDirectory.appendingPathComponent("Games", isDirectory: true)
        if !FileManager.default.fileExists(atPath: dir.path) {
            try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        }
        return dir
    }
    
    public var coversDirectory: URL {
        let dir = documentsDirectory.appendingPathComponent("Covers", isDirectory: true)
        if !FileManager.default.fileExists(atPath: dir.path) {
            try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        }
        return dir
    }
    
    public func loadGames() {
        if let data = UserDefaults.standard.data(forKey: gamesStorageKey),
           let decoded = try? JSONDecoder().decode([GameItem].self, from: data) {
            self.games = decoded
        } else {
            self.games = []
        }
    }
    
    public func saveGames() {
        if let data = try? JSONEncoder().encode(games) {
            UserDefaults.standard.set(data, forKey: gamesStorageKey)
        }
    }
    
    public func parseJad(at url: URL) -> [String: String] {
        var dict: [String: String] = [:]
        if let text = try? String(contentsOf: url, encoding: .utf8) {
            for rawLine in text.components(separatedBy: .newlines) {
                let line = rawLine.trimmingCharacters(in: .whitespaces)
                if line.isEmpty || line.hasPrefix("#") { continue }
                if let colon = line.firstIndex(of: ":") {
                    let k = String(line[..<colon]).trimmingCharacters(in: .whitespaces)
                    let v = String(line[line.index(after: colon)...]).trimmingCharacters(in: .whitespaces)
                    if !k.isEmpty { dict[k] = v }
                }
            }
        }
        return dict
    }

    public func importJar(from sourceURL: URL) {
        let isSecured = sourceURL.startAccessingSecurityScopedResource()
        defer {
            if isSecured { sourceURL.stopAccessingSecurityScopedResource() }
        }

        // JAD descriptor: merge metadata, require sibling JAR with same basename
        if sourceURL.pathExtension.lowercased() == "jad" {
            let jad = parseJad(at: sourceURL)
            let base = sourceURL.deletingPathExtension().lastPathComponent
            let siblingJar = sourceURL.deletingLastPathComponent().appendingPathComponent(base + ".jar")
            if FileManager.default.fileExists(atPath: siblingJar.path) {
                importJar(from: siblingJar)
                return
            }
            // Fallback: JAR already imported? attach JAD title if matches
            if let jarURL = jad["MIDlet-Jar-URL"] {
                let jarName = URL(fileURLWithPath: jarURL).lastPathComponent
                if !jarName.isEmpty {
                    let existing = gamesDirectory.appendingPathComponent(jarName)
                    if FileManager.default.fileExists(atPath: existing.path) { return }
                }
            }
            DispatchQueue.main.async {
                self.errorMessage = "File .jad cần file .jar cùng tên (\(base).jar). Vui lòng chọn file .jar để cài."
                self.showErrorAlert = true
            }
            return
        }
        
        let fileName = sourceURL.lastPathComponent
        let targetURL = gamesDirectory.appendingPathComponent(fileName)
        // Sibling JAD next to picked JAR (Files/iCloud): merge properties
        var jadProps: [String: String] = [:]
        let siblingJad = sourceURL.deletingLastPathComponent().appendingPathComponent(sourceURL.deletingPathExtension().lastPathComponent + ".jad")
        if FileManager.default.fileExists(atPath: siblingJad.path) {
            jadProps = parseJad(at: siblingJad)
        }
        
        do {
            if FileManager.default.fileExists(atPath: targetURL.path) {
                try FileManager.default.removeItem(at: targetURL)
            }
            try FileManager.default.copyItem(at: sourceURL, to: targetURL)
            
            // Extract manifest metadata via J2MEBridge, JAD overrides MANIFEST
            var meta = J2MEBridge.parseJarManifest(targetURL.path)
            for (k, v) in jadProps { meta[k] = v }
            // Persist merged JAD alongside JAR for MIDlet.getAppProperty at runtime
            if !jadProps.isEmpty {
                let targetJad = gamesDirectory.appendingPathComponent(targetURL.deletingPathExtension().lastPathComponent + ".jad")
                var lines: [String] = []
                for (k, v) in jadProps { lines.append("\(k): \(v)") }
                try? lines.joined(separator: "\n").write(to: targetJad, atomically: true, encoding: .utf8)
            }
            let title = meta["MIDlet-Name"] ?? sourceURL.deletingPathExtension().lastPathComponent
            let vendor = meta["MIDlet-Vendor"] ?? "Unknown Vendor"
            let version = meta["MIDlet-Version"] ?? "1.0.0"
            let midlet1 = meta["MIDlet-1"] ?? ""
            
            // Parse main class: format is "Name, /icon.png, com.package.Main"
            var mainClass = ""
            var iconPathInJar: String? = nil
            let parts = midlet1.components(separatedBy: ",")
            if parts.count >= 3 {
                iconPathInJar = parts[1].trimmingCharacters(in: .whitespaces)
                mainClass = parts[2].trimmingCharacters(in: .whitespaces)
            } else if parts.count == 1 {
                mainClass = parts[0].trimmingCharacters(in: .whitespaces)
            }
            
            // Extract icon if found
            var savedIconName: String? = nil
            if let iconPath = iconPathInJar, !iconPath.isEmpty {
                let iconFileName = "\(UUID().uuidString).png"
                let iconTarget = coversDirectory.appendingPathComponent(iconFileName)
                if J2MEBridge.extractJarEntry(targetURL.path, entryName: iconPath, outputPath: iconTarget.path) {
                    savedIconName = iconFileName
                }
            }
            
            let newItem = GameItem(
                title: title,
                vendor: vendor,
                version: version,
                jarFileName: fileName,
                mainClass: mainClass,
                iconFileName: savedIconName,
                dateAdded: Date(),
                config: EmulatorConfig()
            )
            
            DispatchQueue.main.async {
                self.games.insert(newItem, at: 0)
                self.saveGames()
            }
        } catch {
            DispatchQueue.main.async {
                self.errorMessage = "Failed to import JAR: \(error.localizedDescription)"
                self.showErrorAlert = true
            }
        }
    }
    
    public func deleteGame(_ game: GameItem) {
        if let idx = games.firstIndex(where: { $0.id == game.id }) {
            let item = games[idx]
            let jarURL = gamesDirectory.appendingPathComponent(item.jarFileName)
            try? FileManager.default.removeItem(at: jarURL)
            if let iconName = item.iconFileName {
                let iconURL = coversDirectory.appendingPathComponent(iconName)
                try? FileManager.default.removeItem(at: iconURL)
            }
            games.remove(at: idx)
            saveGames()
        }
    }
    
    public func updateGame(_ game: GameItem) {
        if let idx = games.firstIndex(where: { $0.id == game.id }) {
            games[idx] = game
            saveGames()
        }
    }
    
    public func launchGame(_ game: GameItem) {
        var updated = game
        updated.lastPlayed = Date()
        updateGame(updated)
        self.currentGame = updated
        self.isEmulating = true
    }
    
    public func stopEmulation() {
        J2MEBridge.stopEmulator()
        self.isEmulating = false
        self.currentGame = nil
    }
}