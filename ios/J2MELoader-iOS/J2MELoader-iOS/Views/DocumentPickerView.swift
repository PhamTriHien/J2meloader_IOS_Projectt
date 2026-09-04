import SwiftUI
import UniformTypeIdentifiers

public struct DocumentPickerView: UIViewControllerRepresentable {
    public var onPick: (URL) -> Void
    @Environment(\.presentationMode) var presentationMode
    
    public init(onPick: @escaping (URL) -> Void) {
        self.onPick = onPick
    }
    
    public func makeCoordinator() -> Coordinator {
        Coordinator(self)
    }
    
    public func makeUIViewController(context: Context) -> UIDocumentPickerViewController {
        let types: [UTType] = [
            UTType(filenameExtension: "jar") ?? .data,
            UTType(filenameExtension: "jad") ?? .data,
            UTType.zip,
            UTType.data
        ]
        let picker = UIDocumentPickerViewController(forOpeningContentTypes: types, asCopy: true)
        picker.delegate = context.coordinator
        picker.allowsMultipleSelection = false
        return picker
    }
    
    public func updateUIViewController(_ uiViewController: UIDocumentPickerViewController, context: Context) {}
    
    public class Coordinator: NSObject, UIDocumentPickerDelegate {
        let parent: DocumentPickerView
        
        init(_ parent: DocumentPickerView) {
            self.parent = parent
        }
        
        public func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
            guard let url = urls.first else { return }
            parent.onPick(url)
            parent.presentationMode.wrappedValue.dismiss()
        }
        
        public func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) {
            parent.presentationMode.wrappedValue.dismiss()
        }
    }
}