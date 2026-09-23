import SwiftUI
import MetalKit
import UIKit.UIGestureRecognizerSubclass

public struct MetalView: UIViewRepresentable {
    public var config: EmulatorConfig
    public var speedMultiplier: Int
    public var onFpsUpdate: ((Int) -> Void)?
    public var onTouch: (Int32, Int32, Int32) -> Void // x, y, action: 0=down, 1=drag, 2=up
    
    public init(
        config: EmulatorConfig,
        speedMultiplier: Int = 1,
        onFpsUpdate: ((Int) -> Void)? = nil,
        onTouch: @escaping (Int32, Int32, Int32) -> Void
    ) {
        self.config = config
        self.speedMultiplier = speedMultiplier
        self.onFpsUpdate = onFpsUpdate
        self.onTouch = onTouch
    }
    
    public func makeCoordinator() -> MetalRenderer {
        MetalRenderer(self)
    }
    
    public func makeUIView(context: Context) -> GameMTKView {
        let mtkView = GameMTKView()
        mtkView.device = MTLCreateSystemDefaultDevice()
        mtkView.delegate = context.coordinator
        mtkView.enableSetNeedsDisplay = false
        mtkView.isPaused = false
        mtkView.preferredFramesPerSecond = config.targetFps * max(1, speedMultiplier)
        mtkView.clearColor = MTLClearColor(red: 0, green: 0, blue: 0, alpha: 1)
        mtkView.isUserInteractionEnabled = config.touchScreenEnabled
        mtkView.config = config
        mtkView.onTouch = onTouch
        return mtkView
    }
    
    public func updateUIView(_ uiView: GameMTKView, context: Context) {
        uiView.preferredFramesPerSecond = config.targetFps * max(1, speedMultiplier)
        uiView.isUserInteractionEnabled = config.touchScreenEnabled
        uiView.config = config
        uiView.onTouch = onTouch
        context.coordinator.updateConfig(config, speedMultiplier: speedMultiplier, onFpsUpdate: onFpsUpdate)
    }
}

public class GameMTKView: MTKView {
    var onTouch: ((Int32, Int32, Int32) -> Void)?
    var config: EmulatorConfig?
    
    private func handleTouch(_ touch: UITouch?, action: Int32) {
        guard let touch = touch, let cfg = config else { return }
        let point = touch.location(in: self)
        let w = CGFloat(cfg.effectiveWidth)
        let h = CGFloat(cfg.effectiveHeight)
        let viewSize = bounds.size
        if viewSize.width > 0 && viewSize.height > 0 {
            let scaleX = w / viewSize.width
            let scaleY = h / viewSize.height
            let rawX = Int32(point.x * scaleX)
            let rawY = Int32(point.y * scaleY)
            let jx = max(0, min(Int32(w) - 1, rawX))
            let jy = max(0, min(Int32(h) - 1, rawY))
            onTouch?(jx, jy, action)
        }
    }
    
    public override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        handleTouch(touches.first, action: 0)
    }
    
    public override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        handleTouch(touches.first, action: 1)
    }
    
    public override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        handleTouch(touches.first, action: 2)
    }
    
    public override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        handleTouch(touches.first, action: 2)
    }
}

public class MetalRenderer: NSObject, MTKViewDelegate {
    var parent: MetalView
    var device: MTLDevice?
    var commandQueue: MTLCommandQueue?
    var pipelineState: MTLRenderPipelineState?
    var texture: MTLTexture?
    var config: EmulatorConfig
    var speedMultiplier: Int = 1
    var onFpsUpdate: ((Int) -> Void)?
    var lastPaintTick: Int32 = -1
    
    // Real-time FPS measurement
    private var frameCount: Int = 0
    private var lastFpsTimestamp: CFTimeInterval = 0
    public private(set) var currentFps: Int = 0
    
    init(_ parent: MetalView) {
        self.parent = parent
        self.config = parent.config
        self.speedMultiplier = parent.speedMultiplier
        self.onFpsUpdate = parent.onFpsUpdate
        self.device = MTLCreateSystemDefaultDevice()
        if let dev = device {
            self.commandQueue = dev.makeCommandQueue()
        }
        super.init()
        setupPipeline()
    }
    
    func updateConfig(_ config: EmulatorConfig, speedMultiplier: Int, onFpsUpdate: ((Int) -> Void)?) {
        let needPipelineUpdate = self.config.filterMode != config.filterMode
        self.config = config
        self.speedMultiplier = speedMultiplier
        self.onFpsUpdate = onFpsUpdate
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
        let coreW = Int(J2MEBridge.getFrameBufferWidth())
        let coreH = Int(J2MEBridge.getFrameBufferHeight())
        let currentTick = J2MEBridge.getPaintTick()
        
        if let frameBytes = J2MEBridge.getFrameBufferBytes(), coreW > 0, coreH > 0 {
            if texture == nil || texture?.width != coreW || texture?.height != coreH {
                let texDesc = MTLTextureDescriptor.texture2DDescriptor(
                    pixelFormat: .bgra8Unorm,
                    width: coreW,
                    height: coreH,
                    mipmapped: false
                )
                texDesc.usage = [.shaderRead]
                texture = device?.makeTexture(descriptor: texDesc)
                lastPaintTick = -1
            }
            
            // Only update GPU texture when a new frame has actually been rendered
            if currentTick != lastPaintTick {
                let region = MTLRegionMake2D(0, 0, coreW, coreH)
                texture?.replace(region: region, mipmapLevel: 0, withBytes: frameBytes, bytesPerRow: coreW * 4)
                lastPaintTick = currentTick
            }
        }
        
        if let pipeline = pipelineState, let tex = texture {
            encoder.setRenderPipelineState(pipeline)
            encoder.setFragmentTexture(tex, index: 0)
            encoder.drawPrimitives(type: .triangleStrip, vertexStart: 0, vertexCount: 4)
        }
        
        encoder.endEncoding()
        commandBuffer.present(drawable)
        commandBuffer.commit()
        
        // Measure real presented FPS (sliding sample window every 0.5s)
        let now = CACurrentMediaTime()
        if lastFpsTimestamp == 0 {
            lastFpsTimestamp = now
        }
        frameCount += 1
        let elapsed = now - lastFpsTimestamp
        if elapsed >= 0.5 {
            let actualFps = Int(round(Double(frameCount) / elapsed))
            currentFps = actualFps
            frameCount = 0
            lastFpsTimestamp = now
            
            let cb = self.onFpsUpdate
            DispatchQueue.main.async {
                cb?(actualFps)
            }
        }
    }
}