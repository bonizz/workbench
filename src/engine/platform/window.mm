#include "platform/window.h"
#include "core/application.h"

#include "imgui/imgui.h"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

#include <cstdlib>
#include <csignal>
#include <cmath>

@interface WorkbenchView : NSView
{
    NSTimer* timer_;
    NSTimeInterval lastFrameTime_;
    id keyMonitor_;
    NSPoint lastRightMouse_;
    BOOL inRightDrag_;
}
@property(assign, nonatomic) Window* workbenchWindow;
- (instancetype)initWithFrame:(NSRect)frame window:(Window*)window;
@end

@implementation WorkbenchView

- (instancetype)initWithFrame:(NSRect)frame window:(Window*)window {
    self = [super initWithFrame:frame];
    if (!self) return nil;

    self.workbenchWindow = window;

    CAMetalLayer* metalLayer = [CAMetalLayer layer];
    self.wantsLayer = YES;
    self.layer = metalLayer;
    self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    inRightDrag_ = NO;
    lastFrameTime_ = [NSDate timeIntervalSinceReferenceDate];

    __unsafe_unretained WorkbenchView* weakSelf = self;
    keyMonitor_ = [[NSEvent addLocalMonitorForEventsMatchingMask:(NSEventMaskKeyDown | NSEventMaskKeyUp)
        handler:^NSEvent* (NSEvent* event) {
            BOOL isDown = (event.type == NSEventTypeKeyDown);
            BOOL imguiWantsKb = ImGui::GetIO().WantCaptureKeyboard;

            NSString* chars = [event charactersIgnoringModifiers];
            if ([chars length] > 0 && !imguiWantsKb) {
                unichar c = [chars characterAtIndex:0];
                if (Window* w = weakSelf.workbenchWindow) {
                    w->application().onKeyEvent(static_cast<int>(c), isDown);
                }
            }
            return event;
        }] retain];

    timer_ = [[NSTimer scheduledTimerWithTimeInterval:1.0 / 60.0
                                                target:self
                                              selector:@selector(tick)
                                              userInfo:nil
                                               repeats:YES] retain];
    return self;
}

- (void)dealloc {
    [timer_ invalidate];
    [timer_ release];
    [NSEvent removeMonitor:keyMonitor_];
    [keyMonitor_ release];
    [super dealloc];
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)becomeFirstResponder {
    // Redirect first responder to ImGui's KeyEventResponder subview so text
    // input works in ImGui widgets. Game input still flows through the local
    // event monitor installed by the ImGui OSX backend.
    for (NSView* subview in self.subviews) {
        if (subview != self && [subview acceptsFirstResponder]) {
            if ([self.window makeFirstResponder:subview]) {
                return YES;
            }
        }
    }
    return [super becomeFirstResponder];
}

- (void)keyDown:(NSEvent*)event {
    // Swallow key events to prevent system beep. ImGui input is handled by the
    // backend's KeyEventResponder subview; game input is handled by the event
    // monitor.
    (void)event;
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    [self.window makeFirstResponder:self];
    [self resizeRenderer];
    [self tick];
}

- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    [self resizeRenderer];
    [self tick];
}

- (void)setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    if (Window* w = self.workbenchWindow) {
        w->setSize(static_cast<int>(newSize.width), static_cast<int>(newSize.height));
    }
    [self resizeRenderer];
    [self tick];
}

- (void)resizeRenderer {
    if (!self.workbenchWindow) return;
    CGFloat scale = self.window.backingScaleFactor ?: NSScreen.mainScreen.backingScaleFactor;
    self.workbenchWindow->application().onResize(
        static_cast<float>(self.bounds.size.width),
        static_cast<float>(self.bounds.size.height),
        static_cast<float>(scale));
}

- (void)rightMouseDown:(NSEvent*)event {
    inRightDrag_ = YES;
    lastRightMouse_ = [self convertPoint:event.locationInWindow fromView:nil];
}

- (void)rightMouseDragged:(NSEvent*)event {
    if (!inRightDrag_ || !self.workbenchWindow) return;
    NSPoint pt = [self convertPoint:event.locationInWindow fromView:nil];
    float dx = static_cast<float>(pt.x - lastRightMouse_.x);
    float dy = static_cast<float>(lastRightMouse_.y - pt.y); // flip Y
    self.workbenchWindow->application().onMouseDrag(dx, dy);
    lastRightMouse_ = pt;
}

- (void)rightMouseUp:(NSEvent*)event {
    (void)event;
    inRightDrag_ = NO;
}

- (void)mouseDown:(NSEvent*)event {
    if (!self.workbenchWindow) return;
    NSPoint pt = [self convertPoint:event.locationInWindow fromView:nil];
    self.workbenchWindow->application().onMouseButton(0, true,
                                                      static_cast<float>(pt.x),
                                                      static_cast<float>(pt.y));
}

- (void)mouseDragged:(NSEvent*)event {
    if (!self.workbenchWindow) return;
    NSPoint pt = [self convertPoint:event.locationInWindow fromView:nil];
    self.workbenchWindow->application().onLeftMouseMove(static_cast<float>(pt.x),
                                                        static_cast<float>(pt.y));
}

- (void)mouseUp:(NSEvent*)event {
    if (!self.workbenchWindow) return;
    NSPoint pt = [self convertPoint:event.locationInWindow fromView:nil];
    self.workbenchWindow->application().onMouseButton(0, false,
                                                      static_cast<float>(pt.x),
                                                      static_cast<float>(pt.y));
}

- (void)scrollWheel:(NSEvent*)event {
    if (!self.workbenchWindow) return;
    self.workbenchWindow->application().onScroll(static_cast<float>(event.deltaY));
}

- (void)tick {
    if (!self.window || !self.workbenchWindow) return;

    NSTimeInterval now = [NSDate timeIntervalSinceReferenceDate];
    float delta = static_cast<float>(now - lastFrameTime_);
    lastFrameTime_ = now;

    Application& app = self.workbenchWindow->application();
    app.onUpdate(delta);
    app.onRender();
}

@end

@interface WorkbenchAppDelegate : NSObject <NSApplicationDelegate>
@property(retain, nonatomic) NSWindow* window;
@property(assign, nonatomic) Window* workbenchWindow;
@end

@implementation WorkbenchAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
    return YES;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    (void)sender;
    if (Window* w = self.workbenchWindow) {
        w->application().saveSettings();
    }
    return NSTerminateNow;
}

- (void)dealloc {
    [self.window release];
    [super dealloc];
}

@end

struct Window::Impl
{
    Application& app;
    std::string title;
    int width;
    int height;
    NSWindow* window;
    NSView* view;

    Impl(Application& a, const char* t, int w, int h)
        : app(a), title(t ? t : ""), width(w), height(h), window(nil), view(nil)
    {
        NSRect frame = NSMakeRect(0, 0, width, height);
        NSWindowStyleMask style = NSWindowStyleMaskTitled |
                                  NSWindowStyleMaskClosable |
                                  NSWindowStyleMaskMiniaturizable |
                                  NSWindowStyleMaskResizable;

        window = [[NSWindow alloc] initWithContentRect:frame
                                               styleMask:style
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];
        [window setTitle:[NSString stringWithUTF8String:t]];
        view = [[WorkbenchView alloc] initWithFrame:frame window:nil];
        [window setContentView:view];
        [window center];
    }

    ~Impl()
    {
        [view release];
        [window release];
    }
};

Window::Window(Application& app, const char* title, int width, int height)
    : impl_(new Impl(app, title, width, height))
{
    // Now that the Window object exists, wire the view back to it.
    WorkbenchView* view = (WorkbenchView*)impl_->view;
    view.workbenchWindow = this;
}

Window::~Window()
{
    delete impl_;
}

namespace {

void handleTerminationSignal(int)
{
    std::exit(0);
}

} // namespace

void Window::terminate()
{
    [NSApp stop:nil];
}

void Window::run()
{
    NSApplication* app = [NSApplication sharedApplication];
    WorkbenchAppDelegate* delegate = [[WorkbenchAppDelegate alloc] init];
    delegate.window = impl_->window;
    delegate.workbenchWindow = this;

    NSMenu* menuBar = [[NSMenu alloc] init];
    NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
    NSMenu* appMenu = [[NSMenu alloc] initWithTitle:@"Workbench"];
    [appMenu addItemWithTitle:@"Quit Workbench"
                       action:@selector(terminate:)
                keyEquivalent:@"q"];
    [appMenuItem setSubmenu:appMenu];
    [menuBar addItem:appMenuItem];
    [NSApp setMainMenu:menuBar];
    [appMenu release];
    [appMenuItem release];
    [menuBar release];

    app.activationPolicy = NSApplicationActivationPolicyRegular;
    app.delegate = delegate;

    // NSApplication ignores SIGTERM by default. Handle SIGTERM/SIGINT so
    // scripts and manual `kill` terminate the app without leaving it running.
    std::signal(SIGTERM, handleTerminationSignal);
    std::signal(SIGINT, handleTerminationSignal);

    [app run];

    [delegate release];
}

Application& Window::application() const
{
    return impl_->app;
}

void* Window::nativeView() const
{
    return (void*)impl_->view;
}

void* Window::metalLayer() const
{
    return (void*)impl_->view.layer;
}

float Window::width() const
{
    return static_cast<float>(impl_->width);
}

float Window::height() const
{
    return static_cast<float>(impl_->height);
}

float Window::backingScale() const
{
    return static_cast<float>(NSScreen.mainScreen.backingScaleFactor);
}

const char* Window::title() const
{
    return impl_->title.c_str();
}

void Window::setSize(int width, int height)
{
    impl_->width = width;
    impl_->height = height;
}

void Window::setTitle(const char* title)
{
    if (!title) return;
    impl_->title = title;
    [impl_->window setTitle:[NSString stringWithUTF8String:title]];
}

float Window::x() const
{
    return static_cast<float>(impl_->window.frame.origin.x);
}

float Window::y() const
{
    return static_cast<float>(impl_->window.frame.origin.y);
}

void Window::setPosition(int x, int y)
{
    [impl_->window setFrameOrigin:NSMakePoint(x, y)];
}
