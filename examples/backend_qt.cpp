#include <datoviz/datoviz.h>

// #include <QLoggingCategory>
#include <QtGui/QGuiApplication>
#include <QtGui/QVulkanFunctions>
#include <QtGui/QVulkanInstance>
#include <QtGui/QWindow>
// #include <QVulkanWindow>

// #if GCC
// #pragma GCC diagnostic ignored "-Wmissing-declarations"
// #elif CLANG
// #pragma clang diagnostic ignored "-Wmissing-declarations"
// #endif
// // Q_LOGGING_CATEGORY(lcVk, "qt.vulkan")
// #if GCC
// #pragma GCC diagnostic pop
// #elif CLANG
// #pragma clang diagnostic pop
// #endif

class VulkanWindow : public QWindow
{
public:
    VulkanWindow(QVulkanInstance* inst)
    {
        setSurfaceType(VulkanSurface);

        ASSERT(inst != NULL);
        setVulkanInstance(inst);
    }

    void exposeEvent(QExposeEvent*)
    {
        if (isExposed())
        {
            if (!m_initialized)
            {
                m_initialized = true;

                QVulkanInstance* inst = vulkanInstance();
                ASSERT(inst != NULL);

                VkSurfaceKHR surface = inst->surfaceForWindow(this);
                ASSERT(surface != VK_NULL_HANDLE);
            }
        }
    }

    bool event(QEvent* e)
    {
        if (e->type() == QEvent::UpdateRequest)
            render();
        return QWindow::event(e);
    }

    void render()
    {
        requestUpdate(); // render continuously
    }

private:
    bool m_initialized = false;
};



int main(int argc, char** argv)
{
    DvzApp* app = dvz_app(DVZ_BACKEND_QT5);
    DvzGpu* gpu = dvz_gpu(app, 0);

    QGuiApplication qapp(argc, argv);

    QVulkanInstance inst;
    inst.setVkInstance(app->instance);
    if (!inst.create())
    {
        log_error("Vulkan is not available");
        return 1;
    }


    VulkanWindow window(&inst);
    window.show();

    return qapp.exec();
}
