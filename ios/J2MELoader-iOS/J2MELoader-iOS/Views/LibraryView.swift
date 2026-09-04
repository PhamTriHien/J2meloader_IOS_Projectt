import SwiftUI

public struct LibraryView: View {
    @ObservedObject var gameManager: GameManager
    @State private var searchText = ""
    @State private var showingImporter = false
    @State private var selectedGameForSettings: GameItem?
    
    private let columns = [
        GridItem(.adaptive(minimum: 160, maximum: 200), spacing: 16)
    ]
    
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
                Color(.systemGroupedBackground)
                    .ignoresSafeArea()
                
                if gameManager.games.isEmpty {
                    VStack(spacing: 20) {
                        Image(systemName: "gamecontroller.fill")
                            .font(.system(size: 64))
                            .foregroundColor(.secondary)
                        
                        Text("No J2ME Games Installed")
                            .font(.title2.bold())
                            .foregroundColor(.primary)
                        
                        Text("Tap the '+' button to import .jar or .jad files from your Files app, AirDrop, or iCloud.")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                            .multilineTextAlignment(.center)
                            .padding(.horizontal, 40)
                        
                        Button(action: { showingImporter = true }) {
                            Label("Import .JAR Game", systemImage: "plus.circle.fill")
                                .font(.headline)
                                .padding(.horizontal, 24)
                                .padding(.vertical, 12)
                                .background(Color.accentColor)
                                .foregroundColor(.white)
                                .cornerRadius(12)
                        }
                        .padding(.top, 10)
                    }
                } else {
                    ScrollView {
                        LazyVGrid(columns: columns, spacing: 16) {
                            ForEach(filteredGames) { game in
                                GameCardView(game: game, gameManager: gameManager) {
                                    gameManager.launchGame(game)
                                } onSettings: {
                                    selectedGameForSettings = game
                                }
                            }
                        }
                        .padding(16)
                    }
                    .searchable(text: $searchText, prompt: "Search games or vendors...")
                }
            }
            .navigationTitle("J2ME Loader")
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button(action: { showingImporter = true }) {
                        Image(systemName: "plus")
                            .font(.system(size: 18, weight: .semibold))
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
            .fullScreenCover(isPresented: $gameManager.isEmulating) {
                if let current = gameManager.currentGame {
                    GameScreenView(game: current, gameManager: gameManager)
                }
            }
            .alert(isPresented: $gameManager.showErrorAlert) {
                Alert(
                    title: Text("Error"),
                    message: Text(gameManager.errorMessage ?? "An unknown error occurred"),
                    dismissButton: .default(Text("OK"))
                )
            }
        }
    }
}

struct GameCardView: View {
    let game: GameItem
    let gameManager: GameManager
    let onLaunch: () -> Void
    let onSettings: () -> Void
    
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
                        .shadow(color: Color.black.opacity(0.08), radius: 6, x: 0, y: 3)
                    
                    if let img = iconImage {
                        Image(uiImage: img)
                            .interpolation(.none)
                            .resizable()
                            .scaledToFit()
                            .frame(width: 64, height: 64)
                            .cornerRadius(10)
                    } else {
                        Image(systemName: "app.fill")
                            .font(.system(size: 44))
                            .foregroundColor(.blue.opacity(0.8))
                    }
                }
                .frame(height: 120)
                
                VStack(alignment: .leading, spacing: 2) {
                    Text(game.title)
                        .font(.headline)
                        .lineLimit(1)
                        .foregroundColor(.primary)
                    
                    Text(game.vendor)
                        .font(.caption)
                        .lineLimit(1)
                        .foregroundColor(.secondary)
                    
                    Text(game.config.preset.rawValue.components(separatedBy: " ").first ?? "")
                        .font(.caption2.bold())
                        .foregroundColor(.accentColor)
                        .padding(.top, 2)
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
                Label("Play Game", systemImage: "play.fill")
            }
            Button(action: onSettings) {
                Label("Game Settings", systemImage: "gearshape.fill")
            }
            Divider()
            Button(role: .destructive, action: {
                gameManager.deleteGame(game)
            }) {
                Label("Delete", systemImage: "trash")
            }
        }
    }
}