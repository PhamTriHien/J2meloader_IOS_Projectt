import SwiftUI

// MARK: - J2ME-Loader Original Theme Colors
public struct J2MEColors {
    public static let primary = Color(red: 0x21/255.0, green: 0x21/255.0, blue: 0x21/255.0) // #212121
    public static let primaryDark = Color(red: 0x1c/255.0, green: 0x1c/255.0, blue: 0x1c/255.0) // #1c1c1c
    public static let accent = Color(red: 0xff/255.0, green: 0x2e/255.0, blue: 0x51/255.0) // #ff2e51 (Iconic J2ME Red)
    public static let bgLight = Color(red: 0xfa/255.0, green: 0xfa/255.0, blue: 0xfa/255.0) // #fafafa
    public static let bgDark = Color(red: 0x12/255.0, green: 0x12/255.0, blue: 0x12/255.0) // #121212
    public static let cardLight = Color.white
    public static let cardDark = Color(red: 0x1e/255.0, green: 0x1e/255.0, blue: 0x1e/255.0)
}

public struct LibraryView: View {
    @ObservedObject var gameManager: GameManager
    @State private var searchText = ""
    @State private var isSearching = false
    @State private var showingImporter = false
    @State private var selectedGameForSettings: GameItem?
    
    // Dialog states
    @State private var showingAbout = false
    @State private var showingHelp = false
    @State private var showingSettingsGeneral = false
    @State private var gameToRename: GameItem? = nil
    @State private var newGameName: String = ""
    @State private var showingRenameAlert = false
    @State private var gameToClearData: GameItem? = nil
    @State private var showingClearDataAlert = false
    @Environment(\.colorScheme) var colorScheme
    
    public init(gameManager: GameManager) {
        self.gameManager = gameManager
    }
    
    public var filteredGames: [GameItem] {
        if searchText.isEmpty {
            return gameManager.games
        } else {
            return gameManager.games.filter {
                $0.title.localizedCaseInsensitiveContains(searchText) ||
                $0.vendor.localizedCaseInsensitiveContains(searchText)
            }
        }
    }
    
    public var body: some View {
        NavigationView {
            ZStack {
                (colorScheme == .dark ? J2MEColors.bgDark : J2MEColors.bgLight)
                    .ignoresSafeArea()
                
                VStack(spacing: 0) {
                    // Search Bar if active
                    if isSearching {
                        HStack {
                            Image(systemName: "magnifyingglass")
                                .foregroundColor(.secondary)
                            TextField("Search games or vendors...", text: $searchText)
                                .textFieldStyle(PlainTextFieldStyle())
                            if !searchText.isEmpty {
                                Button(action: { searchText = "" }) {
                                    Image(systemName: "xmark.circle.fill")
                                        .foregroundColor(.secondary)
                                }
                            }
                            Button("Cancel") {
                                searchText = ""
                                isSearching = false
                            }
                            .foregroundColor(J2MEColors.accent)
                        }
                        .padding(10)
                        .background(Color(.secondarySystemBackground))
                        .cornerRadius(8)
                        .padding(.horizontal, 12)
                        .padding(.top, 8)
                    }
                    
                    if gameManager.games.isEmpty {
                        // Empty State (Matches Android @string/no_data_for_display)
                        VStack(spacing: 16) {
                            Spacer()
                            Text("No data for display")
                                .font(.system(size: 18, weight: .regular))
                                .foregroundColor(.secondary)
                            Spacer()
                        }
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                    } else {
                        // App List (100% Android ListView Layout list_row_jar.xml)
                        List {
                            ForEach(filteredGames) { game in
                                OriginalListRowJar(
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
                                .listRowInsets(EdgeInsets(top: 0, leading: 0, bottom: 0, trailing: 0))
                                .listRowBackground(Color.clear)
                            }
                        }
                        .listStyle(PlainListStyle())
                    }
                }
                
                // Android Red Floating Action Button (FAB) - Bottom Right (#ff2e51)
                VStack {
                    Spacer()
                    HStack {
                        Spacer()
                        Button(action: { showingImporter = true }) {
                            Image(systemName: "plus")
                                .font(.system(size: 24, weight: .bold))
                                .foregroundColor(.white)
                                .frame(width: 58, height: 58)
                                .background(J2MEColors.accent)
                                .clipShape(Circle())
                                .shadow(color: Color.black.opacity(0.3), radius: 6, x: 0, y: 3)
                        }
                        .padding(.trailing, 20)
                        .padding(.bottom, 24)
                    }
                }
            }
            // Android Dark Material Toolbar (#212121)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .principal) {
                    HStack {
                        Text("J2ME-Loader")
                            .font(.system(size: 19, weight: .bold))
                            .foregroundColor(.white)
                        Spacer()
                    }
                }
                
                ToolbarItem(placement: .navigationBarTrailing) {
                    HStack(spacing: 16) {
                        Button(action: { isSearching.toggle() }) {
                            Image(systemName: "magnifyingglass")
                                .font(.system(size: 17, weight: .semibold))
                                .foregroundColor(.white)
                        }
                        
                        Menu {
                            Button(action: { showingSettingsGeneral = true }) {
                                Label("Settings", systemImage: "gearshape")
                            }
                            Button(action: { showingHelp = true }) {
                                Label("Help", systemImage: "questionmark.circle")
                            }
                            Button(action: { showingAbout = true }) {
                                Label("About", systemImage: "info.circle")
                            }
                        } label: {
                            Image(systemName: "ellipsis")
                                .rotationEffect(.degrees(90))
                                .font(.system(size: 17, weight: .bold))
                                .foregroundColor(.white)
                        }
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
            .sheet(isPresented: $showingHelp) { HelpView() }
            .sheet(isPresented: $showingAbout) { AboutView() }
            .fullScreenCover(isPresented: $gameManager.isEmulating) {
                if let current = gameManager.currentGame {
                    GameScreenView(game: current, gameManager: gameManager)
                }
            }
            .alert("Change MIDlet Name", isPresented: $showingRenameAlert) {
                TextField("MIDlet Name", text: $newGameName)
                Button("OK") {
                    if let target = gameToRename, !newGameName.isEmpty {
                        var updated = target
                        updated.title = newGameName
                        gameManager.updateGame(updated)
                    }
                }
                Button("Cancel", role: .cancel) {}
            }
            .alert("Clear Saved Data?", isPresented: $showingClearDataAlert) {
                Button("Clear", role: .destructive) {
                    if let target = gameToClearData {
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
                Text("Delete RMS storage records for \(gameToClearData?.title ?? "")?")
            }
        }
        .navigationViewStyle(StackNavigationViewStyle())
    }
}

// MARK: - Original Android List Row (list_row_jar.xml)
struct OriginalListRowJar: View {
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
            HStack(alignment: .center, spacing: 12) {
                // 36dip x 36dip Icon (Original Android Layout size)
                if let img = iconImage {
                    Image(uiImage: img)
                        .interpolation(.none)
                        .resizable()
                        .scaledToFill()
                        .frame(width: 38, height: 38)
                        .cornerRadius(4)
                } else {
                    ZStack {
                        Color(red: 0x52/255.0, green: 0x5a/255.0, blue: 0xa0/255.0)
                        Image(systemName: "app.fill")
                            .font(.system(size: 22))
                            .foregroundColor(.white)
                    }
                    .frame(width: 38, height: 38)
                    .cornerRadius(4)
                }
                
                // Vertical metadata container
                VStack(alignment: .leading, spacing: 3) {
                    Text(game.title)
                        .font(.system(size: 15, weight: .bold))
                        .foregroundColor(.primary)
                        .lineLimit(1)
                    
                    HStack {
                        Text(game.vendor)
                            .font(.system(size: 12))
                            .foregroundColor(.secondary)
                            .lineLimit(1)
                        
                        Spacer()
                        
                        Text(game.version)
                            .font(.system(size: 12))
                            .foregroundColor(.secondary)
                    }
                }
            }
            .padding(.horizontal, 14)
            .padding(.vertical, 10)
        }
        .buttonStyle(PlainButtonStyle())
        .contextMenu {
            Button(action: onLaunch) { Label("Start", systemImage: "play.fill") }
            Button(action: onSettings) { Label("Settings", systemImage: "gearshape.fill") }
            Button(action: onRename) { Label("Rename", systemImage: "pencil") }
            Button(action: onClearData) { Label("Clear Data", systemImage: "trash.slash") }
            Divider()
            Button(role: .destructive, action: { gameManager.deleteGame(game) }) {
                Label("Delete", systemImage: "trash")
            }
        }
    }
}

// MARK: - About View (AboutDialogFragment.java)
struct AboutView: View {
    @Environment(\.presentationMode) var presentationMode
    
    var body: some View {
        NavigationView {
            Form {
                Section {
                    VStack(spacing: 8) {
                        Image(systemName: "gamecontroller.fill")
                            .font(.system(size: 48))
                            .foregroundColor(J2MEColors.accent)
                        
                        Text("J2ME-Loader")
                            .font(.system(size: 20, weight: .bold))
                        
                        Text("Version 1.8.2")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 8)
                }
                
                Section(header: Text("Author")) {
                    HStack {
                        Text("Nikita Shakarun")
                        Spacer()
                        Text("PlaySoftware").foregroundColor(.secondary)
                    }
                }
                
                Section(header: Text("License")) {
                    Text("Licensed under the Apache License, Version 2.0")
                        .font(.footnote)
                        .foregroundColor(.secondary)
                }
            }
            .navigationTitle("About")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("OK") { presentationMode.wrappedValue.dismiss() }
                        .foregroundColor(J2MEColors.accent)
                        .font(.headline)
                }
            }
        }
    }
}

// MARK: - Help View (HelpDialogFragment.java)
struct HelpView: View {
    @Environment(\.presentationMode) var presentationMode
    
    var body: some View {
        NavigationView {
            Form {
                Section(header: Text("Usage")) {
                    Text("Tap '+' button to install .jar or .jad file.\nLong-press an app in the list to open the context menu.")
                        .font(.subheadline)
                }
                
                Section(header: Text("Controls")) {
                    Text("Virtual keypad can be configured in app settings. Touchscreen is supported for games that support pointer events.")
                        .font(.subheadline)
                }
            }
            .navigationTitle("Help")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("OK") { presentationMode.wrappedValue.dismiss() }
                        .foregroundColor(J2MEColors.accent)
                        .font(.headline)
                }
            }
        }
    }
}