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
                    // Thanh tìm kiếm
                    if isSearching {
                        HStack {
                            Image(systemName: "magnifyingglass")
                                .foregroundColor(.secondary)
                            TextField("Tìm kiếm game hoặc nhà phát triển...", text: $searchText)
                                .textFieldStyle(PlainTextFieldStyle())
                            if !searchText.isEmpty {
                                Button(action: { searchText = "" }) {
                                    Image(systemName: "xmark.circle.fill")
                                        .foregroundColor(.secondary)
                                }
                            }
                            Button("Huỷ") {
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
                        // Trạng thái thư viện trống
                        VStack(spacing: 16) {
                            Spacer()
                            Image(systemName: "square.grid.2x2")
                                .font(.system(size: 48))
                                .foregroundColor(.secondary.opacity(0.6))
                            Text("Chưa có ứng dụng nào trong thư viện")
                                .font(.system(size: 17, weight: .semibold))
                                .foregroundColor(.primary)
                            Text("Nhấn vào nút '+' màu đỏ ở góc dưới để cài đặt\nfile game Java (.jar hoặc .jad) từ máy của bạn.")
                                .font(.system(size: 14))
                                .multilineTextAlignment(.center)
                                .foregroundColor(.secondary)
                                .padding(.horizontal, 32)
                            Spacer()
                        }
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                    } else {
                        // Danh sách ứng dụng chuẩn list_row_jar
                        List {
                            ForEach(filteredGames) { game in
                                OriginalListRowJar(
                                    game: game,
                                    gameManager: gameManager,
                                    onLaunch: {
                                        let quickLaunch = UserDefaults.standard.bool(forKey: "J2ME_QUICK_LAUNCH")
                                        if quickLaunch {
                                            gameManager.launchGame(game)
                                        } else {
                                            selectedGameForSettings = game
                                        }
                                    },
                                    onDirectPlay: {
                                        gameManager.launchGame(game)
                                    },
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
                
                // Nút nổi FAB thêm game màu đỏ (#ff2e51)
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
            // Thanh công cụ Dark Toolbar (#212121)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .principal) {
                    HStack {
                        Text("J2HienLoader")
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
                                Label("Cài đặt chung", systemImage: "gearshape")
                            }
                            Button(action: { showingHelp = true }) {
                                Label("Hướng dẫn sử dụng", systemImage: "questionmark.circle")
                            }
                            Button(action: { showingAbout = true }) {
                                Label("Thông tin ứng dụng", systemImage: "info.circle")
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
                SettingsView(
                    game: game,
                    onSave: { updated in
                        gameManager.updateGame(updated)
                    },
                    onStart: { updated in
                        gameManager.updateGame(updated)
                        gameManager.launchGame(updated)
                    }
                )
            }
            .sheet(isPresented: $showingSettingsGeneral) { GeneralSettingsView() }
            .sheet(isPresented: $showingHelp) { HelpView() }
            .sheet(isPresented: $showingAbout) { AboutView() }
            .fullScreenCover(isPresented: $gameManager.isEmulating) {
                if let current = gameManager.currentGame {
                    GameScreenView(game: current, gameManager: gameManager)
                }
            }
            .alert("Đổi tên ứng dụng", isPresented: $showingRenameAlert) {
                TextField("Tên mới của game", text: $newGameName)
                Button("Đồng ý") {
                    if let target = gameToRename, !newGameName.isEmpty {
                        var updated = target
                        updated.title = newGameName
                        gameManager.updateGame(updated)
                    }
                }
                Button("Huỷ", role: .cancel) {}
            }
            .alert("Xóa dữ liệu lưu (RMS)?", isPresented: $showingClearDataAlert) {
                Button("Xóa dữ liệu", role: .destructive) {
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
                Button("Huỷ", role: .cancel) {}
            } message: {
                Text("Toàn bộ dữ liệu điểm cao và màn chơi đã lưu của '\(gameToClearData?.title ?? "")' sẽ bị xóa vĩnh viễn.")
            }
        }
        .navigationViewStyle(StackNavigationViewStyle())
    }
}

// MARK: - Hàng hiển thị game chuẩn (list_row_jar.xml)
struct OriginalListRowJar: View {
    let game: GameItem
    let gameManager: GameManager
    let onLaunch: () -> Void
    let onDirectPlay: () -> Void
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
                        
                        Text("v\(game.version)")
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
            Button(action: onDirectPlay) { Label("Bắt đầu chơi", systemImage: "play.fill") }
            Button(action: onSettings) { Label("Cài đặt game", systemImage: "gearshape.fill") }
            Button(action: onRename) { Label("Đổi tên", systemImage: "pencil") }
            Button(action: onClearData) { Label("Xóa dữ liệu (RMS)", systemImage: "trash.slash") }
            Divider()
            Button(role: .destructive, action: { gameManager.deleteGame(game) }) {
                Label("Xóa khỏi thư viện", systemImage: "trash")
            }
        }
    }
}

// MARK: - Cài đặt chung (General Settings Dialog)
struct GeneralSettingsView: View {
    @AppStorage("J2ME_QUICK_LAUNCH") private var quickLaunch: Bool = false
    @AppStorage("J2ME_BG_KEEP_ALIVE") private var bgKeepAlive: Bool = true
    @AppStorage("J2ME_NETWORK_KEEP_ALIVE") private var networkKeepAlive: Bool = true
    @Environment(\.presentationMode) var presentationMode
    
    var body: some View {
        NavigationView {
            Form {
                Section(header: Text("Tùy chọn khởi chạy")) {
                    Toggle("Khởi chạy nhanh khi bấm vào game", isOn: $quickLaunch)
                    Text("Khi bật, chạm vào game trong thư viện sẽ vào chơi ngay lập tức mà không hiện hộp thoại cài đặt.")
                        .font(.footnote)
                        .foregroundColor(.secondary)
                }
                
                Section(header: Text("Tối ưu hệ thống & Chạy ngầm 24/7 (Anti-Crash)")) {
                    Toggle("Treo game ngầm 24/7 không bị tắt", isOn: $bgKeepAlive)
                    Toggle("Giữ kết nối mạng Socket liên tục", isOn: $networkKeepAlive)
                }
                
                Section(header: Text("Bộ nhớ & Dữ liệu")) {
                    HStack {
                        Text("Thư mục dữ liệu RMS")
                        Spacer()
                        Text("Documents/RMS").foregroundColor(.secondary)
                    }
                }
            }
            .navigationTitle("Cài đặt chung")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Đóng") { presentationMode.wrappedValue.dismiss() }
                        .foregroundColor(J2MEColors.accent)
                        .font(.headline)
                }
            }
        }
    }
}

// MARK: - Thông tin ứng dụng (About Dialog)
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
                        
                        Text("J2HienLoader")
                            .font(.system(size: 22, weight: .bold))
                        
                        Text("Phiên bản 1.8.2 (Tiếng Việt)")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 8)
                }
                
                Section(header: Text("Tác giả & Đóng góp")) {
                    HStack {
                        Text("Tác giả bản gốc Android")
                        Spacer()
                        Text("Nikita Shakarun (PlaySoftware)").foregroundColor(.secondary)
                    }
                    HStack {
                        Text("Phát triển bản iOS")
                        Spacer()
                        Text("Phạm Trí Hiện").foregroundColor(.secondary).fontWeight(.semibold)
                    }
                    HStack {
                        Text("Đồ họa & Âm thanh")
                        Spacer()
                        Text("Apple Metal & Sonivox EAS").foregroundColor(.secondary)
                    }
                }
                
                Section(header: Text("Giấy phép")) {
                    Text("Phát hành theo giấy phép mã nguồn mở Apache License 2.0")
                        .font(.footnote)
                        .foregroundColor(.secondary)
                }
            }
            .navigationTitle("Thông tin ứng dụng")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Đóng") { presentationMode.wrappedValue.dismiss() }
                        .foregroundColor(J2MEColors.accent)
                        .font(.headline)
                }
            }
        }
    }
}

// MARK: - Hướng dẫn sử dụng (Help Dialog)
struct HelpView: View {
    @Environment(\.presentationMode) var presentationMode
    
    var body: some View {
        NavigationView {
            Form {
                Section(header: Text("Cách cài đặt game Java (.jar / .jad)")) {
                    Text("1. Nhấn vào nút '+' màu đỏ ở góc dưới bên phải màn hình.\n2. Chọn file .jar hoặc .jad từ ứng dụng Tệp (Files), iCloud Drive hoặc tải trực tiếp từ trình duyệt Safari.\n3. Trò chơi sẽ tự động xuất hiện trong thư viện với đầy đủ biểu tượng và thông tin.")
                        .font(.subheadline)
                }
                
                Section(header: Text("Cách điều khiển khi chơi")) {
                    Text("• Bàn phím ảo: Dùng cụm phím số cổ điển (1-9, *, 0, #), phím điều hướng D-Pad và 2 phím mềm LSK/RSK.\n• Cảm ứng trực tiếp: Chạm hoặc vuốt trực tiếp trên màn hình game.\n• Tay cầm Bluetooth: Hỗ trợ tay cầm PS5, Xbox, MFi và có thể gán phím trong phần Cài đặt.")
                        .font(.subheadline)
                }
                
                Section(header: Text("Hiệu năng & Âm thanh")) {
                    Text("• Tăng tốc phần cứng Metal GPU duy trì mượt mà 60 FPS.\n• Bộ tổng hợp Sonivox EAS tái tạo chân thực âm thanh nhạc chuông MIDI đa âm sắc cổ điển.")
                        .font(.subheadline)
                }
            }
            .navigationTitle("Hướng dẫn sử dụng")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Đóng") { presentationMode.wrappedValue.dismiss() }
                        .foregroundColor(J2MEColors.accent)
                        .font(.headline)
                }
            }
        }
    }
}