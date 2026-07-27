#pragma once

#include "LeirEngine/Core/CoreObject.h"

namespace Leir {

class LEIR_API Object2D : public CoreObject {
public:
    Object2D(const std::string& name = "Object2D");
    ~Object2D() override = default;

    int GetSortingLayer() const { return m_SortingLayer; }
    void SetSortingLayer(int layer) { m_SortingLayer = layer; }

    int GetOrderInLayer() const { return m_OrderInLayer; }
    void SetOrderInLayer(int order) { m_OrderInLayer = order; }

private:
    int m_SortingLayer = 0;
    int m_OrderInLayer = 0;
};

} // namespace Leir
