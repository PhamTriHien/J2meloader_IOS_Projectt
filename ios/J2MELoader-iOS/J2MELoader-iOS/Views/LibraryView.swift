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

public enum ActiveSheet: Identifiable {
    case importer
    case settings(GameItem)
    case generalSettings
    case help
    case about
    
    public var id: String {
        switch self {
        case .importer: return "importer"
        case .settings(let game): return "settings_\(game.id.uuidString)"
        case .generalSettings: return "general"
        case .help: return "help"
        case .about: return "about"
        }
    }
}

public struct LibraryView: View {
    @ObservedObject var gameManager: GameManager
    @ObservedObject var updateManager = AppUpdateManager.shared
    @State private var searchText = ""
    @State private var isSearching = false
    @State private var activeSheet: ActiveSheet? = nil
    
    // Dialog states
    @State private var gameToRename: GameItem? = nil
    @State private var newGameName: String = ""
    @State private var showingRenameAlert = false
    @State private var gameToClearData: GameItem? = nil
    @State private var showingClearDataAlert = false
    @Environment(\.colorScheme) var colorScheme
    
    public init(gameManager: GameManager) {
        _gameManager = ObservedObject(wrappedValue: gameManager)
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
        ZStack {
            NavigationView {
                ZStack {
                (colorScheme == .dark ? J2MEColors.bgDark : J2MEColors.bgLight)
                    .ignoresSafeArea()
                
                VStack(spacing: 0) {
                    // Thanh tìm kiếm
                    if isSearching {
                        HStack(spacing: 8) {
                            Image(systemName: "magnifyingglass")
                                .font(.system(size: 13.5, weight: .semibold))
                                .foregroundColor(.secondary)
                            TextField("Tìm kiếm game hoặc nhà phát triển...", text: $searchText)
                                .font(.system(size: 13, weight: .regular))
                                .textFieldStyle(PlainTextFieldStyle())
                            if !searchText.isEmpty {
                                Button(action: { searchText = "" }) {
                                    Image(systemName: "xmark.circle.fill")
                                        .font(.system(size: 13.5))
                                        .foregroundColor(.secondary)
                                }
                            }
                            Button("Huỷ") {
                                searchText = ""
                                isSearching = false
                            }
                            .font(.system(size: 13, weight: .medium))
                            .foregroundColor(J2MEColors.accent)
                        }
                        .padding(.horizontal, 10)
                        .padding(.vertical, 8)
                        .background(Color(.secondarySystemBackground))
                        .cornerRadius(8)
                        .padding(.horizontal, 12)
                        .padding(.top, 8)
                    }
                    
                    if gameManager.games.isEmpty {
                        // Trạng thái thư viện trống
                        VStack(spacing: 14) {
                            Spacer()
                            Image(systemName: "square.grid.2x2")
                                .font(.system(size: 42))
                                .foregroundColor(.secondary.opacity(0.6))
                            Text("Chưa có ứng dụng nào trong thư viện")
                                .font(.system(size: 15.5, weight: .semibold))
                                .foregroundColor(.primary)
                            Text("Nhấn vào nút '+' màu đỏ ở góc dưới để cài đặt\nfile game Java (.jar hoặc .jad) từ máy của bạn.")
                                .font(.system(size: 12.5, weight: .regular))
                                .multilineTextAlignment(.center)
                                .foregroundColor(.secondary)
                                .padding(.horizontal, 28)
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
                                            activeSheet = .settings(game)
                                        }
                                    },
                                    onDirectPlay: {
                                        gameManager.launchGame(game)
                                    },
                                    onSettings: { activeSheet = .settings(game) },
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
                        Button(action: { activeSheet = .importer }) {
                            Image(systemName: "plus")
                                .font(.system(size: 22, weight: .bold))
                                .foregroundColor(.white)
                                .frame(width: 54, height: 54)
                                .background(J2MEColors.accent)
                                .clipShape(Circle())
                                .shadow(color: Color.black.opacity(0.28), radius: 5, x: 0, y: 3)
                        }
                        .padding(.trailing, 18)
                        .padding(.bottom, 20)
                    }
                }
            }
            // Thanh công cụ Dark Toolbar (#212121)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .principal) {
                    HStack {
                        Text("J2HienLoader")
                            .font(.system(size: 17, weight: .bold))
                            .foregroundColor(.white)
                        Spacer()
                    }
                }
                
                ToolbarItem(placement: .navigationBarTrailing) {
                    HStack(spacing: 16) {
                        Button(action: { isSearching.toggle() }) {
                            Image(systemName: "magnifyingglass")
                                .font(.system(size: 15, weight: .semibold))
                                .foregroundColor(.white)
                        }
                        
                        Menu {
                            Button(action: { activeSheet = .generalSettings }) {
                                Label("Cài đặt chung", systemImage: "gearshape")
                            }
                            Button(action: { activeSheet = .help }) {
                                Label("Hướng dẫn sử dụng", systemImage: "questionmark.circle")
                            }
                            Button(action: { activeSheet = .about }) {
                                Label("Thông tin ứng dụng", systemImage: "info.circle")
                            }
                        } label: {
                            Image(systemName: "ellipsis")
                                .rotationEffect(.degrees(90))
                                .font(.system(size: 15, weight: .bold))
                                .foregroundColor(.white)
                        }
                    }
                }
            }
            .sheet(item: $activeSheet) { sheet in
                switch sheet {
                case .importer:
                    DocumentPickerView { url in
                        gameManager.importJar(from: url)
                    }
                case .settings(let game):
                    SettingsView(
                        game: game,
                        onSave: { updated in
                            gameManager.updateGame(updated)
                        },
                        onStart: { updated in
                            activeSheet = nil
                            gameManager.updateGame(updated)
                            gameManager.launchGame(updated)
                        }
                    )
                case .generalSettings:
                    GeneralSettingsView()
                case .help:
                    HelpView()
                case .about:
                    AboutView()
                }
            }
            .alert("Đổi tên ứng dụng", isPresented: $showingRenameAlert) {
                TextField("Tên mới của game", text: $newGameName)
                Button("Đồng ý") {
                    if let target = gameToRename, !newGameName.trimmingCharacters(in: .whitespaces).isEmpty {
                        var updated = target
                        updated.title = newGameName.trimmingCharacters(in: .whitespaces)
                        gameManager.updateGame(updated)
                    }
                }
                Button("Huỷ", role: .cancel) {}
            }
            .background(
                EmptyView()
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
            )
            .sheet(isPresented: $updateManager.showingUpdateModal) {
                UpdateModalView()
            }
            .onAppear {
                updateManager.checkForUpdates(manual: false)
            }
        }
        .navigationViewStyle(StackNavigationViewStyle())
        
        if gameManager.isEmulating, let current = gameManager.currentGame {
            GameScreenView(game: current, gameManager: gameManager)
                .transition(.opacity)
                .zIndex(100)
        }
    }
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
                        .cornerRadius(6)
                } else {
                    ZStack {
                        Color(red: 0x52/255.0, green: 0x5a/255.0, blue: 0xa0/255.0)
                        Image(systemName: "app.fill")
                            .font(.system(size: 20))
                            .foregroundColor(.white)
                    }
                    .frame(width: 38, height: 38)
                    .cornerRadius(6)
                }
                
                VStack(alignment: .leading, spacing: 3) {
                    Text(game.title)
                        .font(.system(size: 14.5, weight: .bold))
                        .foregroundColor(.primary)
                        .lineLimit(1)
                    
                    HStack {
                        Text(game.vendor)
                            .font(.system(size: 11.5, weight: .regular))
                            .foregroundColor(.secondary)
                            .lineLimit(1)
                        
                        Spacer()
                        
                        Text("v\(game.version)")
                            .font(.system(size: 11, weight: .medium))
                            .foregroundColor(.secondary)
                    }
                }
            }
            .padding(.horizontal, 14)
            .padding(.vertical, 9)
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
    @ObservedObject private var updateManager = AppUpdateManager.shared
    @Environment(\.presentationMode) var presentationMode
    
    var body: some View {
        NavigationView {
            Form {
                Section(header: Text("TÙY CHỌN KHỞI CHẠY").font(.system(size: 11.5, weight: .semibold))) {
                    Toggle(isOn: $quickLaunch) {
                        Text("Khởi chạy nhanh khi bấm vào game")
                            .font(.system(size: 13.5, weight: .regular))
                    }
                    Text("Khi bật, chạm vào game trong thư viện sẽ vào chơi ngay lập tức mà không hiện hộp thoại cài đặt.")
                        .font(.system(size: 11.5, weight: .regular))
                        .foregroundColor(.secondary)
                }
                
                Section(header: Text("TỐI ƯU HỆ THỐNG & CHẠY NGẦM 24/7").font(.system(size: 11.5, weight: .semibold))) {
                    Toggle(isOn: $bgKeepAlive) {
                        Text("Treo game ngầm 24/7 không bị tắt")
                            .font(.system(size: 13.5, weight: .regular))
                    }
                    Toggle(isOn: $networkKeepAlive) {
                        Text("Giữ kết nối mạng Socket liên tục")
                            .font(.system(size: 13.5, weight: .regular))
                    }
                }
                
                Section(header: Text("TỰ ĐỘNG CẬP NHẬT & BẢN VÁ").font(.system(size: 11.5, weight: .semibold))) {
                    Toggle(isOn: $updateManager.autoCheckUpdates) {
                        Text("Tự động kiểm tra bản vá khi có mạng")
                            .font(.system(size: 13.5, weight: .regular))
                    }
                    
                    Button(action: {
                        updateManager.checkForUpdates(manual: true)
                    }) {
                        HStack {
                            Text("Kiểm tra bản cập nhật ngay")
                                .font(.system(size: 13.5, weight: .medium))
                                .foregroundColor(J2MEColors.accent)
                            Spacer()
                            if updateManager.isChecking {
                                ProgressView()
                                    .scaleEffect(0.8)
                            } else {
                                Image(systemName: "arrow.clockwise")
                                    .font(.system(size: 13))
                                    .foregroundColor(J2MEColors.accent)
                            }
                        }
                    }
                    
                    if let status = updateManager.statusMessage {
                        Text(status)
                            .font(.system(size: 12, weight: .regular))
                            .foregroundColor(updateManager.hasUpdate ? J2MEColors.accent : .secondary)
                    }
                }
                
                Section(header: Text("BỘ NHỚ & DỮ LIỆU").font(.system(size: 11.5, weight: .semibold))) {
                    HStack {
                        Text("Thư mục dữ liệu RMS")
                            .font(.system(size: 13.5, weight: .regular))
                        Spacer()
                        Text("Documents/RMS")
                            .font(.system(size: 12.5, weight: .regular))
                            .foregroundColor(.secondary)
                    }
                }
            }
            .navigationTitle("Cài đặt chung")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Đóng") { presentationMode.wrappedValue.dismiss() }
                        .foregroundColor(J2MEColors.accent)
                        .font(.system(size: 14, weight: .bold))
                }
            }
        }
        .navigationViewStyle(StackNavigationViewStyle())
    }
}

// MARK: - Thông tin ứng dụng (About Dialog)
struct AboutView: View {
    @Environment(\.presentationMode) var presentationMode
    
    var body: some View {
        NavigationView {
            Form {
                Section {
                    VStack(spacing: 6) {
                        Image(systemName: "gamecontroller.fill")
                            .font(.system(size: 42))
                            .foregroundColor(J2MEColors.accent)
                        
                        Text("J2HienLoader")
                            .font(.system(size: 20, weight: .bold))
                        
                        Text("Phiên bản 1.8.2 (Tiếng Việt)")
                            .font(.system(size: 12.5, weight: .medium))
                            .foregroundColor(.secondary)
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 6)
                }
                
                Section(header: Text("TÁC GIẢ & ĐÓNG GÓP").font(.system(size: 11.5, weight: .semibold))) {
                    HStack {
                        Text("Tác giả bản gốc Android")
                            .font(.system(size: 13, weight: .regular))
                        Spacer()
                        Text("Nikita Shakarun")
                            .font(.system(size: 12.5, weight: .regular))
                            .foregroundColor(.secondary)
                    }
                    HStack {
                        Text("Phát triển bản iOS")
                            .font(.system(size: 13, weight: .regular))
                        Spacer()
                        Text("Phạm Trí Hiện")
                            .font(.system(size: 12.5, weight: .semibold))
                            .foregroundColor(.secondary)
                    }
                    HStack {
                        Text("Đồ họa & Âm thanh")
                            .font(.system(size: 13, weight: .regular))
                        Spacer()
                        Text("Apple Metal & Sonivox EAS")
                            .font(.system(size: 12.5, weight: .regular))
                            .foregroundColor(.secondary)
                    }
                }
                
                Section(header: Text("GIẤY PHÉP").font(.system(size: 11.5, weight: .semibold))) {
                    Text("Phát hành theo giấy phép mã nguồn mở Apache License 2.0")
                        .font(.system(size: 11.5, weight: .regular))
                        .foregroundColor(.secondary)
                }
            }
            .navigationTitle("Thông tin ứng dụng")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Đóng") { presentationMode.wrappedValue.dismiss() }
                        .foregroundColor(J2MEColors.accent)
                        .font(.system(size: 14, weight: .bold))
                }
            }
        }
        .navigationViewStyle(StackNavigationViewStyle())
    }
}

// MARK: - Hướng dẫn sử dụng (Help Dialog)
struct HelpView: View {
    @Environment(\.presentationMode) var presentationMode
    
    var body: some View {
        NavigationView {
            Form {
                Section(header: Text("CÁCH CÀI ĐẶT GAME JAVA (.JAR / .JAD)").font(.system(size: 11.5, weight: .semibold))) {
                    Text("1. Nhấn vào nút '+' màu đỏ ở góc dưới bên phải màn hình.\n2. Chọn file .jar hoặc .jad từ ứng dụng Tệp (Files), iCloud Drive hoặc tải trực tiếp từ Safari.\n3. Trò chơi sẽ tự động xuất hiện trong thư viện với đầy đủ biểu tượng và thông tin.")
                        .font(.system(size: 12.5, weight: .regular))
                }
                
                Section(header: Text("CÁCH ĐIỀU KHIỂN KHI CHƠI").font(.system(size: 11.5, weight: .semibold))) {
                    Text("• Bàn phím ảo: Dùng cụm phím số cổ điển (1-9, *, 0, #), phím điều hướng D-Pad và 2 phím mềm LSK/RSK.\n• Cảm ứng trực tiếp: Chạm hoặc vuốt trực tiếp trên màn hình game.\n• Tay cầm Bluetooth: Hỗ trợ tay cầm PS5, Xbox, MFi và có thể gán phím trong phần Cài đặt.")
                        .font(.system(size: 12.5, weight: .regular))
                }
                
                Section(header: Text("HIỆU NĂNG & ÂM THANH").font(.system(size: 11.5, weight: .semibold))) {
                    Text("• Tăng tốc phần cứng Metal GPU duy trì mượt mà 60 FPS.\n• Bộ tổng hợp Sonivox EAS tái tạo chân thực âm thanh nhạc chuông MIDI đa âm sắc cổ điển.")
                        .font(.system(size: 12.5, weight: .regular))
                }
            }
            .navigationTitle("Hướng dẫn sử dụng")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Đóng") { presentationMode.wrappedValue.dismiss() }
                        .foregroundColor(J2MEColors.accent)
                        .font(.system(size: 14, weight: .bold))
                }
            }
        }
        .navigationViewStyle(StackNavigationViewStyle())
    }
}