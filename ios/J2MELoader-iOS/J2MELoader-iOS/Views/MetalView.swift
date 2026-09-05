import SwiftUI
import MetalKit
import UIKit.UIGestureRecognizerSubclass

public struct MetalView: UIViewRepresentable {
    public var config: EmulatorConfig
    public var onTouch: (Int32, Int32, Int32) -> Void // x, y, action: 0=down, 1=drag, 2=up
    
    public init(config: EmulatorConfig, onTouch: @escaping (Int32, Int32, Int32) -> Void) {
        self.config = config
        self.onTouch = onTouch
    }
    
    public func makeCoordinator() -> MetalRenderer {
        MetalRenderer(self)
    }
    
    public func makeUIView(context: Context) -> MTKView {
        let mtkView = MTKView()
        mtkView.device = MTLCreateSystemDefaultDevice()
        mtkView.delegate = context.coordinator
        mtkView.enableSetNeedsDisplay = false
        mtkView.isPaused = false
        mtkView.preferredFramesPerSecond = config.targetFps
        mtkView.clearColor = MTLClearColor(red: 0, green: 0, blue: 0, alpha: 1)
        mtkView.isUserInteractionEnabled = config.touchScreenEnabled
        
        let touchHandler = TouchGestureRecognizer { point, action in
            let w = CGFloat(self.config.effectiveWidth)
            let h = CGFloat(self.config.effectiveHeight)
            let viewSize = mtkView.bounds.size
            if viewSize.width > 0 && viewSize.height > 0 {
                let scaleX = w / viewSize.width
                let scaleY = h / viewSize.height
                let jx = Int32(point.x * scaleX)
                let jy = Int32(point.y * scaleY)
                self.onTouch(jx, jy, action)
            }
        }
        mtkView.addGestureRecognizer(touchHandler)
        return mtkView
    }
    
    public func updateUIView(_ uiView: MTKView, context: Context) {
        uiView.preferredFramesPerSecond = config.targetFps
        context.coordinator.updateConfig(config)
    }
}

public class MetalRenderer: NSObject, MTKViewDelegate {
    var parent: MetalView
    var device: MTLDevice?
    var commandQueue: MTLCommandQueue?
    var pipelineState: MTLRenderPipelineState?
    var texture: MTLTexture?
    var config: EmulatorConfig
    
    init(_ parent: MetalView) {
        self.parent = parent
        self.config = parent.config
        self.device = MTLCreateSystemDefaultDevice()
        if let dev = device {
            self.commandQueue = dev.makeCommandQueue()
        }
        super.init()
        setupPipeline()
    }
    
    func updateConfig(_ config: EmulatorConfig) {
        let needPipelineUpdate = self.config.filterMode != config.filterMode
        self.config = config
        if needPipelineUpdate {
            setupPipeline()
        }
    }
    
    func setupPipeline() {
        guard let device = device else { return }
        let defaultLibrary = device.makeDefaultLibrary()
        let vertexFunc = defaultLibrary?.makeFunction(name: "vertexShader")
        
        let fragName: String
        switch config.filterMode {
        case .crtScanlines:
            fragName = "crtFragmentShader"
        case .lcdGrid:
            fragName = "lcdGridFragmentShader"
        default:
            fragName = "fragmentShader"
        }
        let fragmentFunc = defaultLibrary?.makeFunction(name: fragName)
        
        let pipelineDescriptor = MTLRenderPipelineDescriptor()
        pipelineDescriptor.vertexFunction = vertexFunc
        pipelineDescriptor.fragmentFunction = fragmentFunc
        pipelineDescriptor.colorAttachments[0].pixelFormat = .bgra8Unorm
        
        pipelineState = try? device.makeRenderPipelineState(descriptor: pipelineDescriptor)
    }
    
    public func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}
    
    public func draw(in view: MTKView) {
        guard let drawable = view.currentDrawable,
              let renderPassDesc = view.currentRenderPassDescriptor,
              let commandQueue = commandQueue,
              let commandBuffer = commandQueue.makeCommandBuffer(),
              let encoder = commandBuffer.makeRenderCommandEncoder(descriptor: renderPassDesc) else {
            return
        }
        
        // Fetch current RGB buffer from J2ME Core
        let width = config.effectiveWidth
        let height = config.effectiveHeight
        
        if let frameBytes = J2MEBridge.getFrameBufferBytes() {
            if texture == nil || texture?.width != width || texture?.height != height {
                let texDesc = MTLTextureDescriptor.texture2DDescriptor(
                    pixelFormat: .rgba8Unorm,
                    width: width,
                    height: height,
                    mipmapped: false
                )
                texDesc.usage = [.shaderRead]
                texture = device?.makeTexture(descriptor: texDesc)
            }
            
            let region = MTLRegionMake2D(0, 0, width, height)
            texture?.replace(region: region, mipmapLevel: 0, withBytes: frameBytes, bytesPerRow: width * 4)
        }
        
        if let pipeline = pipelineState, let tex = texture {
            encoder.setRenderPipelineState(pipeline)
            encoder.setFragmentTexture(tex, index: 0)
            encoder.drawPrimitives(type: .triangleStrip, vertexStart: 0, vertexCount: 4)
        }
        
        encoder.endEncoding()
        commandBuffer.present(drawable)
        commandBuffer.commit()
    }
}

class TouchGestureRecognizer: UIGestureRecognizer {
    var onTouch: (CGPoint, Int32) -> Void
    
    init(onTouch: @escaping (CGPoint, Int32) -> Void) {
        self.onTouch = onTouch
        super.init(target: nil, action: nil)
    }
    
    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent) {
        state = .began
        if let touch = touches.first {
            let point = touch.location(in: view)
            onTouch(point, 0)
        }
    }
    
    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent) {
        state = .changed
        if let touch = touches.first {
            let point = touch.location(in: view)
            onTouch(point, 1)
        }
    }
    
    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent) {
        state = .ended
        if let touch = touches.first {
            let point = touch.location(in: view)
            onTouch(point, 2)
        }
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent) {
        state = .cancelled
        if let touch = touches.first {
            let point = touch.location(in: view)
            onTouch(point, 2)
        }
    }
}