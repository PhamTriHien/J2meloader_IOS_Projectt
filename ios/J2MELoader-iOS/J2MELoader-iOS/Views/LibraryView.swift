import SwiftUI

public enum ViewMode: String, CaseIterable {
    case grid = "square.grid.2x2"
    case list = "list.bullet"
}

public enum SortOrder: String, CaseIterable {
    case name = "Title"
    case vendor = "Vendor"
    case dateAdded = "Date Added"
    case lastPlayed = "Last Played"
}

public struct LibraryView: View {
    @ObservedObject var gameManager: GameManager
    @State private var searchText = ""
    @State private var showingImporter = false
    @State private var selectedGameForSettings: GameItem?
    @State private var viewMode: ViewMode = .grid
    @State private var sortOrder: SortOrder = .name
    
    // Dialog states
    @State private var showingAbout = false
    @State private var showingHelp = false
    @State private var gameToRename: GameItem? = nil
    @State private var newGameName: String = ""
    @State private var showingRenameAlert = false
    @State private var gameToClearData: GameItem? = nil
    @State private var showingClearDataAlert = false
    
    private let columns = [
        GridItem(.adaptive(minimum: 155, maximum: 200), spacing: 14)
    ]
    
    public init(gameManager: GameManager) {
        self.gameManager = gameManager
    }
    
    public var sortedAndFilteredGames: [GameItem] {
        var result = gameManager.games
        
        if !searchText.isEmpty {
            result = result.filter {
                $0.title.localizedCaseInsensitiveContains(searchText) ||
                $0.vendor.localizedCaseInsensitiveContains(searchText)
            }
        }
        
        switch sortOrder {
        case .name:
            result.sort { $0.title.localizedCaseInsensitiveCompare($1.title) == .orderedAscending }
        case .vendor:
            result.sort { $0.vendor.localizedCaseInsensitiveCompare($1.vendor) == .orderedAscending }
        case .dateAdded:
            result.sort { $0.dateAdded > $1.dateAdded }
        case .lastPlayed:
            result.sort { ($0.lastPlayed ?? Date.distantPast) > ($1.lastPlayed ?? Date.distantPast) }
        }
        
        return result
    }
    
    public var body: some View {
        NavigationView {
            ZStack {
                Color(.systemGroupedBackground)
                    .ignoresSafeArea()
                
                if gameManager.games.isEmpty {
                    VStack(spacing: 20) {
                        Image(systemName: "gamecontroller.fill")
                            .font(.system(size: 68))
                            .foregroundColor(.accentColor.opacity(0.8))
                        
                        Text("No J2ME Games Installed")
                            .font(.title2.bold())
                            .foregroundColor(.primary)
                        
                        Text("Tap the '+' button or drag and drop to import your favorite Java ME (.jar / .jad) retro games.")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                            .multilineTextAlignment(.center)
                            .padding(.horizontal, 40)
                        
                        Button(action: { showingImporter = true }) {
                            Label("Import .JAR / .JAD Game", systemImage: "plus.circle.fill")
                                .font(.headline)
                                .padding(.horizontal, 24)
                                .padding(.vertical, 14)
                                .background(Color.accentColor)
                                .foregroundColor(.white)
                                .cornerRadius(14)
                                .shadow(color: Color.accentColor.opacity(0.3), radius: 8, x: 0, y: 4)
                        }
                        .padding(.top, 8)
                    }
                } else {
                    ScrollView {
                        if viewMode == .grid {
                            LazyVGrid(columns: columns, spacing: 14) {
                                ForEach(sortedAndFilteredGames) { game in
                                    GameCardGridItem(
                                        game: game,
                                        gameManager: gameManager,
                                        onLaunch: { gameManager.launchGame(game) },
                                        onSettings: { selectedGameForSettings = game },
                                        onRename: {
                                            gameToRename = game
                                            newGameName = game.title
                                            showingRenameAlert = true
                                        },
                                        onClearData: {
                                            gameToClearData = game
                                            showingClearDataAlert = true
                                        }
                                    )
                                }
                            }
                            .padding(14)
                        } else {
                            LazyVStack(spacing: 8) {
                                ForEach(sortedAndFilteredGames) { game in
                                    GameCardListItem(
                                        game: game,
                                        gameManager: gameManager,
                                        onLaunch: { gameManager.launchGame(game) },
                                        onSettings: { selectedGameForSettings = game },
                                        onRename: {
                                            gameToRename = game
                                            newGameName = game.title
                                            showingRenameAlert = true
                                        },
                                        onClearData: {
                                            gameToClearData = game
                                            showingClearDataAlert = true
                                        }
                                    )
                                }
                            }
                            .padding(14)
                        }
                    }
                    .searchable(text: $searchText, prompt: "Search games or vendors...")
                }
            }
            .navigationTitle("J2ME Loader")
            .toolbar {
                ToolbarItem(placement: .navigationBarLeading) {
                    Menu {
                        Picker("View Mode", selection: $viewMode) {
                            Label("Grid View", systemImage: "square.grid.2x2").tag(ViewMode.grid)
                            Label("List View", systemImage: "list.bullet").tag(ViewMode.list)
                        }
                        
                        Divider()
                        
                        Picker("Sort by", selection: $sortOrder) {
                            Label("Title", systemImage: "textformat").tag(SortOrder.name)
                            Label("Vendor", systemImage: "building.2").tag(SortOrder.vendor)
                            Label("Date Added", systemImage: "calendar").tag(SortOrder.dateAdded)
                            Label("Last Played", systemImage: "clock").tag(SortOrder.lastPlayed)
                        }
                        
                        Divider()
                        
                        Button(action: { showingHelp = true }) {
                            Label("Help & Guide", systemImage: "questionmark.circle")
                        }
                        
                        Button(action: { showingAbout = true }) {
                            Label("About J2ME Loader", systemImage: "info.circle")
                        }
                    } label: {
                        Image(systemName: "ellipsis.circle")
                            .font(.system(size: 18, weight: .semibold))
                    }
                }
                
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button(action: { showingImporter = true }) {
                        Image(systemName: "plus")
                            .font(.system(size: 18, weight: .bold))
                    }
                }
            }
            .sheet(isPresented: $showingImporter) {
                DocumentPickerView { url in
                    gameManager.importJar(from: url)
                }
            }
            .sheet(item: $selectedGameForSettings) { game in
                SettingsView(game: game) { updated in
                    gameManager.updateGame(updated)
                }
            }
            .sheet(isPresented: $showingHelp) {
                HelpView()
            }
            .sheet(isPresented: $showingAbout) {
                AboutView()
            }
            .fullScreenCover(isPresented: $gameManager.isEmulating) {
                if let current = gameManager.currentGame {
                    GameScreenView(game: current, gameManager: gameManager)
                }
            }
            .alert("Rename Game", isPresented: $showingRenameAlert) {
                TextField("Game Name", text: $newGameName)
                Button("Save") {
                    if let target = gameToRename, !newGameName.isEmpty {
                        var updated = target
                        updated.title = newGameName
                        gameManager.updateGame(updated)
                    }
                }
                Button("Cancel", role: .cancel) {}
            } message: {
                Text("Enter a new title for this game:")
            }
            .alert("Clear Saved Game Data?", isPresented: $showingClearDataAlert) {
                Button("Clear RMS Data", role: .destructive) {
                    if let target = gameToClearData {
                        // Clear RMS files for this game
                        let rmsDir = gameManager.documentsDirectory.appendingPathComponent("RMS")
                        let pattern = "\(target.title)_"
                        if let files = try? FileManager.default.contentsOfDirectory(atPath: rmsDir.path) {
                            for file in files where file.contains(pattern) {
                                try? FileManager.default.removeItem(at: rmsDir.appendingPathComponent(file))
                            }
                        }
                    }
                }
                Button("Cancel", role: .cancel) {}
            } message: {
                Text("This will permanently delete all savegames and settings stored in RMS flash memory for '\(gameToClearData?.title ?? "")'.")
            }
        }
    }
}

// MARK: - Grid Card Item
struct GameCardGridItem: View {
    let game: GameItem
    let gameManager: GameManager
    let onLaunch: () -> Void
    let onSettings: () -> Void
    let onRename: () -> Void
    let onClearData: () -> Void
    
    var iconImage: UIImage? {
        if let iconName = game.iconFileName {
            let iconURL = gameManager.coversDirectory.appendingPathComponent(iconName)
            return UIImage(contentsOfFile: iconURL.path)
        }
        return nil
    }
    
    var body: some View {
        Button(action: onLaunch) {
            VStack(alignment: .leading, spacing: 8) {
                ZStack {
                    RoundedRectangle(cornerRadius: 14)
                        .fill(Color(.secondarySystemGroupedBackground))
                        .shadow(color: Color.black.opacity(0.06), radius: 5, x: 0, y: 2)
                    
                    if let img = iconImage {
                        Image(uiImage: img)
                            .interpolation(.none)
                            .resizable()
                            .scaledToFit()
                            .frame(width: 64, height: 64)
                            .cornerRadius(10)
                    } else {
                        Image(systemName: "app.fill")
                            .font(.system(size: 42))
                            .foregroundColor(.accentColor.opacity(0.8))
                    }
                }
                .frame(height: 110)
                
                VStack(alignment: .leading, spacing: 2) {
                    Text(game.title)
                        .font(.system(size: 14, weight: .bold))
                        .lineLimit(1)
                        .foregroundColor(.primary)
                    
                    Text(game.vendor)
                        .font(.system(size: 11))
                        .lineLimit(1)
                        .foregroundColor(.secondary)
                    
                    Text(game.config.preset.rawValue.components(separatedBy: " ").first ?? "")
                        .font(.system(size: 10, weight: .semibold))
                        .foregroundColor(.accentColor)
                        .padding(.top, 1)
                }
                .padding(.horizontal, 4)
            }
            .padding(10)
            .background(Color(.secondarySystemGroupedBackground))
            .cornerRadius(16)
        }
        .buttonStyle(PlainButtonStyle())
        .contextMenu {
            Button(action: onLaunch) {
                Label("Start Game", systemImage: "play.fill")
            }
            Button(action: onSettings) {
                Label("Game Settings", systemImage: "gearshape.fill")
            }
            Button(action: onRename) {
                Label("Rename", systemImage: "pencil")
            }
            Button(action: onClearData) {
                Label("Clear Saved Data (RMS)", systemImage: "trash.slash")
            }
            Divider()
            Button(role: .destructive, action: {
                gameManager.deleteGame(game)
            }) {
                Label("Delete Game", systemImage: "trash.fill")
            }
        }
    }
}

// MARK: - List Card Item
struct GameCardListItem: View {
    let game: GameItem
    let gameManager: GameManager
    let onLaunch: () -> Void
    let onSettings: () -> Void
    let onRename: () -> Void
    let onClearData: () -> Void
    
    var iconImage: UIImage? {
        if let iconName = game.iconFileName {
            let iconURL = gameManager.coversDirectory.appendingPathComponent(iconName)
            return UIImage(contentsOfFile: iconURL.path)
        }
        return nil
    }
    
    var body: some View {
        Button(action: onLaunch) {
            HStack(spacing: 12) {
                if let img = iconImage {
                    Image(uiImage: img)
                        .interpolation(.none)
                        .resizable()
                        .scaledToFit()
                        .frame(width: 48, height: 48)
                        .cornerRadius(8)
                } else {
                    Image(systemName: "app.fill")
                        .font(.system(size: 36))
                        .foregroundColor(.accentColor.opacity(0.8))
                        .frame(width: 48, height: 48)
                }
                
                VStack(alignment: .leading, spacing: 3) {
                    Text(game.title)
                        .font(.system(size: 15, weight: .bold))
                        .lineLimit(1)
                        .foregroundColor(.primary)
                    
                    Text("\(game.vendor) • v\(game.version)")
                        .font(.system(size: 12))
                        .lineLimit(1)
                        .foregroundColor(.secondary)
                }
                
                Spacer()
                
                Button(action: onSettings) {
                    Image(systemName: "gearshape")
                        .font(.system(size: 18))
                        .foregroundColor(.secondary)
                        .padding(8)
                }
                .buttonStyle(BorderlessButtonStyle())
            }
            .padding(12)
            .background(Color(.secondarySystemGroupedBackground))
            .cornerRadius(12)
        }
        .buttonStyle(PlainButtonStyle())
        .contextMenu {
            Button(action: onLaunch) { Label("Start Game", systemImage: "play.fill") }
            Button(action: onSettings) { Label("Game Settings", systemImage: "gearshape.fill") }
            Button(action: onRename) { Label("Rename", systemImage: "pencil") }
            Button(action: onClearData) { Label("Clear Saved Data (RMS)", systemImage: "trash.slash") }
            Divider()
            Button(role: .destructive, action: { gameManager.deleteGame(game) }) { Label("Delete Game", systemImage: "trash.fill") }
        }
    }
}

// MARK: - About View
struct AboutView: View {
    @Environment(\.presentationMode) var presentationMode
    
    var body: some View {
        NavigationView {
            Form {
                Section {
                    VStack(spacing: 12) {
                        Image(systemName: "gamecontroller.fill")
                            .font(.system(size: 54))
                            .foregroundColor(.accentColor)
                        
                        Text("J2ME Loader for iOS")
                            .font(.title2.bold())
                        
                        Text("Version 1.8.2 Native Port")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                        
                        Text("A high-performance feature-complete J2ME (Java Micro Edition) emulator for iPhone & iPad powered by Swift, Metal & CoreAudio.")
                            .font(.footnote)
                            .multilineTextAlignment(.center)
                            .foregroundColor(.secondary)
                            .padding(.top, 4)
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 12)
                }
                
                Section(header: Text("Credits & Open Source")) {
                    HStack {
                        Text("Original Android Author")
                        Spacer()
                        Text("Nikita Shakarun (PlaySoftware)").foregroundColor(.secondary)
                    }
                    HStack {
                        Text("iOS Native Engine")
                        Spacer()
                        Text("Metal LCDUI & Sonivox EAS").foregroundColor(.secondary)
                    }
                    HStack {
                        Text("License")
                        Spacer()
                        Text("Apache 2.0 / GNU GPL").foregroundColor(.secondary)
                    }
                }
            }
            .navigationTitle("About")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Done") { presentationMode.wrappedValue.dismiss() }
                }
            }
        }
    }
}

// MARK: - Help View
struct HelpView: View {
    @Environment(\.presentationMode) var presentationMode
    
    var body: some View {
        NavigationView {
            Form {
                Section(header: Text("How to add games")) {
                    Text("1. Tap the '+' button in the top right corner.\n2. Select any .jar or .jad file from your Files app, iCloud Drive, or AirDrop.\n3. The game will automatically appear in your library with its icon and metadata.")
                        .font(.subheadline)
                }
                
                Section(header: Text("Game Controls")) {
                    Text("• On-Screen Keypad: Use the retro keypad (1-9, *, 0, #, D-Pad, LSK, RSK).\n• Direct Touchscreen: Tap directly on the canvas for touch games.\n• Bluetooth Gamepad: Connect any Xbox, PlayStation, Switch, or MFi controller and configure buttons in Settings -> Key Mapper.")
                        .font(.subheadline)
                }
                
                Section(header: Text("Audio & Performance")) {
                    Text("• Sonivox EAS MIDI engine reproduces vintage polyphonic ringtones and SoundFont music.\n• Metal hardware acceleration maintains smooth 60 FPS gameplay.")
                        .font(.subheadline)
                }
            }
            .navigationTitle("Help & Guide")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Done") { presentationMode.wrappedValue.dismiss() }
                }
            }
        }
    }
}