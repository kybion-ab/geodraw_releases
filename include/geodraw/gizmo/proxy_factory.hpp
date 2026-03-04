/*******************************************************************************
 * File: gizmo/proxy_factory.hpp
 *
 * Description: Type-erased proxy factory system for auto-registering editable
 *              objects. Maps geometry types to their corresponding proxy types.
 *
 * Usage:
 *   // Built-in registrations happen automatically
 *   if (ProxyRegistry::canCreate<Shape3>()) {
 *       auto proxy = ProxyRegistry::create(&myShape);
 *       editCtx.addProxy(std::move(proxy));
 *   }
 *
 *   // Custom registrations
 *   ProxyRegistry::registerProxy<MyType, MyTypeProxy>();
 *
 ******************************************************************************/

#pragma once

#include "edit_proxy.hpp"
#include <memory>
#include <typeindex>
#include <unordered_map>

namespace geodraw {

/**
 * Type-erased base class for proxy factories
 */
class ProxyFactoryBase {
public:
    virtual ~ProxyFactoryBase() = default;
    virtual std::unique_ptr<EditProxyBase> create(void* obj) = 0;
};

/**
 * Typed proxy factory
 *
 * @tparam T Type of the geometry object
 * @tparam ProxyT Type of the proxy (must derive from EditProxy<T>)
 */
template<typename T, typename ProxyT>
class ProxyFactory : public ProxyFactoryBase {
public:
    std::unique_ptr<EditProxyBase> create(void* obj) override {
        return std::make_unique<ProxyT>(static_cast<T*>(obj));
    }
};

/**
 * Global registry for proxy factories
 *
 * Maps geometry types (via std::type_index) to their corresponding
 * proxy factory. Built-in types are registered automatically.
 */
class ProxyRegistry {
public:
    /**
     * Register a proxy type for a geometry type
     *
     * @tparam T The geometry type (e.g., Shape3)
     * @tparam ProxyT The proxy type (e.g., Shape3Proxy)
     */
    template<typename T, typename ProxyT>
    static void registerProxy() {
        std::type_index typeIdx(typeid(T));
        factories()[typeIdx] = std::make_unique<ProxyFactory<T, ProxyT>>();
    }

    /**
     * Check if a proxy can be created for a geometry type
     *
     * @tparam T The geometry type to check
     * @return True if a proxy factory is registered for this type
     */
    template<typename T>
    static bool canCreate() {
        ensureInitialized();
        std::type_index typeIdx(typeid(T));
        return factories().count(typeIdx) > 0;
    }

    /**
     * Create a proxy for a geometry object
     *
     * @tparam T The geometry type
     * @param obj Pointer to the geometry object
     * @return Unique pointer to the created proxy, or nullptr if no factory registered
     */
    template<typename T>
    static std::unique_ptr<EditProxyBase> create(T* obj) {
        ensureInitialized();
        std::type_index typeIdx(typeid(T));
        auto it = factories().find(typeIdx);
        if (it != factories().end()) {
            return it->second->create(obj);
        }
        return std::unique_ptr<EditProxyBase>();
    }

private:
    // Singleton factory map
    static std::unordered_map<std::type_index, std::unique_ptr<ProxyFactoryBase>>& factories() {
        static std::unordered_map<std::type_index, std::unique_ptr<ProxyFactoryBase>> instance;
        return instance;
    }

    // Separate initialization flag to avoid recursive static initialization
    static bool& initialized() {
        static bool init = false;
        return init;
    }

    // Ensure built-in proxies are registered (called by public methods)
    static void ensureInitialized() {
        if (!initialized()) {
            initialized() = true;  // Set BEFORE registering to prevent recursion

            // Register Shape3 -> Shape3Proxy
            registerProxy<Shape3, Shape3Proxy>();

            // Future registrations:
            // registerProxy<Mesh3, Mesh3Proxy>();
            // registerProxy<Polyline3, PolylineProxy>();
            // registerProxy<Pose3, PoseProxy>();
            // registerProxy<BBox3, BBoxProxy>();
        }
    }
};

} // namespace geodraw
