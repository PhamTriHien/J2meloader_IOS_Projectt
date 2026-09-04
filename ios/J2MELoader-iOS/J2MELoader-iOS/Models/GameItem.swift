import Foundation
import SwiftUI

public struct GameItem: Identifiable, Codable, Hashable {
    public var id: UUID
    public var title: String
    public var vendor: String
    public var version: String
    public var jarFileName: String
    public var mainClass: String
    public var iconFileName: String?
    public var dateAdded: Date
    public var lastPlayed: Date?
    public var config: EmulatorConfig
    
    public init(
        id: UUID = UUID(),
        title: String,
        vendor: String = "Unknown Vendor",
        version: String = "1.0.0",
        jarFileName: String,
        mainClass: String = "",
        iconFileName: String? = nil,
        dateAdded: Date = Date(),
        lastPlayed: Date? = nil,
        config: EmulatorConfig = EmulatorConfig()
    ) {
        self.id = id
        self.title = title
        self.vendor = vendor
        self.version = version
        self.jarFileName = jarFileName
        self.mainClass = mainClass
        self.iconFileName = iconFileName
        self.dateAdded = dateAdded
        self.lastPlayed = lastPlayed
        self.config = config
    }
}