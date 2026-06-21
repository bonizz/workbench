#pragma once

class Application;

class Window
{
public:
    Window(Application& app, const char* title, int width, int height);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void run();
    void terminate();

    void* nativeView() const;
    void* metalLayer() const;

    float width() const;
    float height() const;

    void setSize(int width, int height);
    float backingScale() const;
    const char* title() const;

    Application& application() const;

private:
    struct Impl;
    Impl* impl_;
};
