#include "bvh.hpp"
#include "GpuWrappers/cl_context.hpp"
#include "Utils/cl_exception.hpp"

namespace
{
    constexpr auto kMaxPrimitivesInNode = 4u;
}

Bvh::Bvh(CLContext& cl_context)
    : AccelerationStructure(cl_context)
{
    intersect_kernel_ = std::make_unique<CLKernel>("src/Kernels/trace_bvh.cl", cl_context);
}

void Bvh::BuildCPU(std::vector<Triangle> & triangles)
{
    std::cout << "Building Bounding Volume Hierarchy for scene" << std::endl;

    std::uint32_t max_prims_in_node = 4u;

    //double startTime = m_Render.GetCurtime();
    std::vector<BVHPrimitiveInfo> primitiveInfo(triangles.size());
    for (unsigned int i = 0; i < triangles.size(); ++i)
    {
        primitiveInfo[i] = { i, triangles[i].GetBounds() };
    }

    unsigned int totalNodes = 0;
    std::vector<Triangle> orderedTriangles;
    root_node_ = RecursiveBuild(triangles, primitiveInfo, 0, triangles.size(), &totalNodes, orderedTriangles);
    triangles.swap(orderedTriangles);

    //primitiveInfo.resize(0);
    std::cout << "BVH created with " << totalNodes << " nodes for " << triangles.size()
        << " triangles (" << float(totalNodes * sizeof(BVHBuildNode)) / (1024.0f * 1024.0f) << " MB, "
        //<< m_Render.GetCurtime() - startTime << "s elapsed)"
        << std::endl;

    // Compute representation of depth-first traversal of BVH tree
    nodes_.resize(totalNodes);
    unsigned int offset = 0;
    FlattenBVHTree(root_node_, &offset);
    assert(totalNodes == offset);

    // Upload GPU data
    cl_int status;
    nodes_buffer_ = cl::Buffer(cl_context_.GetContext(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        nodes_.size() * sizeof(LinearBVHNode), nodes_.data(), &status);
    if (status != CL_SUCCESS)
    {
        throw CLException("Failed to create BVH node buffer", status);
    }

    triangles_buffer_ = cl::Buffer(cl_context_.GetContext(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        triangles.size() * sizeof(Triangle), triangles.data(), &status);
    if (status != CL_SUCCESS)
    {
        throw CLException("Failed to create BVH triangles buffer", status);
    }

}

/*
void DrawTree(BVHBuildNode* node, float x, float y, int depth)
{
    float size_x = 0.025f;
    float size_y = 0.03f;
    if (node->children[0])
    {
        DrawTree(node->children[0], x - 1.0f / y, y + 1, depth * 2);
    }

    if (node->nPrimitives == 0)
    {
        glColor3f(1, 0, 0);
    }
    else
    {
        glColor3f(0, 1, 0);
    }

    glRectf(x / 4 - size_x, 1 - y / 8 - size_y, x / 4 + size_x, 1 - y / 8 + size_y);

    if (node->children[1])
    {
        DrawTree(node->children[1], x + 1.0f / y, y + 1, depth * 2);
    }
}

void Bvh::DrawDebug()
{
    // XYZ Arrows
    glColor3f(1, 0, 0);
    glLineWidth(4);
    glBegin(GL_LINE_LOOP);
    glColor3f(1, 0, 0);
    glVertex3f(0, 0, 0);
    glVertex3f(200, 0, 0);
    glEnd();
    glBegin(GL_LINE_LOOP);
    glColor3f(0, 1, 0);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 200, 0);
    glEnd();
    glBegin(GL_LINE_LOOP);
    glColor3f(0, 0, 1);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 0, 200);
    glEnd();
    glColor3f(1, 0, 0);
    glLineWidth(1);

    glColor3f(0, 1, 0);
    glBegin(GL_LINES);
    for (unsigned int i = 0; i < nodes_.size(); ++i)
    {

        glVertex3f(nodes_[i].bounds.min.x, nodes_[i].bounds.min.y, nodes_[i].bounds.min.z);
        glVertex3f(nodes_[i].bounds.max.x, nodes_[i].bounds.min.y, nodes_[i].bounds.min.z);

        glVertex3f(nodes_[i].bounds.min.x, nodes_[i].bounds.min.y, nodes_[i].bounds.min.z);
        glVertex3f(nodes_[i].bounds.min.x, nodes_[i].bounds.max.y, nodes_[i].bounds.min.z);

        glVertex3f(nodes_[i].bounds.min.x, nodes_[i].bounds.min.y, nodes_[i].bounds.min.z);
        glVertex3f(nodes_[i].bounds.min.x, nodes_[i].bounds.min.y, nodes_[i].bounds.max.z);

        glVertex3f(nodes_[i].bounds.min.x, nodes_[i].bounds.min.y, nodes_[i].bounds.max.z);
        glVertex3f(nodes_[i].bounds.max.x, nodes_[i].bounds.min.y, nodes_[i].bounds.max.z);

        glVertex3f(nodes_[i].bounds.min.x, nodes_[i].bounds.min.y, nodes_[i].bounds.max.z);
        glVertex3f(nodes_[i].bounds.min.x, nodes_[i].bounds.max.y, nodes_[i].bounds.max.z);

        glVertex3f(nodes_[i].bounds.min.x, nodes_[i].bounds.max.y, nodes_[i].bounds.min.z);
        glVertex3f(nodes_[i].bounds.min.x, nodes_[i].bounds.max.y, nodes_[i].bounds.max.z);

        glVertex3f(nodes_[i].bounds.min.x, nodes_[i].bounds.max.y, nodes_[i].bounds.min.z);
        glVertex3f(nodes_[i].bounds.max.x, nodes_[i].bounds.max.y, nodes_[i].bounds.min.z);

        glVertex3f(nodes_[i].bounds.max.x, nodes_[i].bounds.min.y, nodes_[i].bounds.min.z);
        glVertex3f(nodes_[i].bounds.max.x, nodes_[i].bounds.max.y, nodes_[i].bounds.min.z);

        glVertex3f(nodes_[i].bounds.max.x, nodes_[i].bounds.min.y, nodes_[i].bounds.min.z);
        glVertex3f(nodes_[i].bounds.max.x, nodes_[i].bounds.min.y, nodes_[i].bounds.max.z);

        glVertex3f(nodes_[i].bounds.max.x, nodes_[i].bounds.max.y, nodes_[i].bounds.min.z);
        glVertex3f(nodes_[i].bounds.max.x, nodes_[i].bounds.max.y, nodes_[i].bounds.max.z);

        glVertex3f(nodes_[i].bounds.max.x, nodes_[i].bounds.min.y, nodes_[i].bounds.max.z);
        glVertex3f(nodes_[i].bounds.max.x, nodes_[i].bounds.max.y, nodes_[i].bounds.max.z);

        glVertex3f(nodes_[i].bounds.min.x, nodes_[i].bounds.max.y, nodes_[i].bounds.max.z);
        glVertex3f(nodes_[i].bounds.max.x, nodes_[i].bounds.max.y, nodes_[i].bounds.max.z);


    }

    glEnd();


    //DrawTree(root_node_, 0.0f, 1.0f, 1);

}
*/

Bvh::BVHBuildNode* Bvh::RecursiveBuild(
    std::vector<Triangle> const& triangles,
    std::vector<BVHPrimitiveInfo>& primitiveInfo,
    unsigned int start,
    unsigned int end, unsigned int* totalNodes,
    std::vector<Triangle>& orderedTriangles)
{
    assert(start <= end);

    //// TODO: Add memory pool
    BVHBuildNode* node = new BVHBuildNode;
    (*totalNodes)++;

    // Compute bounds of all primitives in BVH node
    Bounds3 bounds;
    for (unsigned int i = start; i < end; ++i)
    {
        bounds = Union(bounds, primitiveInfo[i].bounds);
    }

    unsigned int nPrimitives = end - start;
    if (nPrimitives == 1)
    {
        // Create leaf
        int firstPrimOffset = orderedTriangles.size();
        for (unsigned int i = start; i < end; ++i)
        {
            int primNum = primitiveInfo[i].primitiveNumber;
            orderedTriangles.push_back(triangles[primNum]);
        }
        node->InitLeaf(firstPrimOffset, nPrimitives, bounds);
        return node;
    }
    else
    {
        // Compute bound of primitive centroids, choose split dimension
        Bounds3 centroidBounds;
        for (unsigned int i = start; i < end; ++i)
        {
            centroidBounds = Union(centroidBounds, primitiveInfo[i].centroid);
        }
        unsigned int dim = centroidBounds.MaximumExtent();

        // Partition primitives into two sets and build children
        unsigned int mid = (start + end) / 2;
        if (centroidBounds.max[dim] == centroidBounds.min[dim])
        {
            // Create leaf
            unsigned int firstPrimOffset = orderedTriangles.size();
            for (unsigned int i = start; i < end; ++i)
            {
                unsigned int primNum = primitiveInfo[i].primitiveNumber;
                orderedTriangles.push_back(triangles[primNum]);
            }
            node->InitLeaf(firstPrimOffset, nPrimitives, bounds);
            return node;
        }
        else
        {
            if (nPrimitives <= 2)
            {
                // Partition primitives into equally-sized subsets
                mid = (start + end) / 2;
                std::nth_element(&primitiveInfo[start], &primitiveInfo[mid], &primitiveInfo[end - 1] + 1,
                    [dim](const BVHPrimitiveInfo& a, const BVHPrimitiveInfo& b)
                    {
                        return a.centroid[dim] < b.centroid[dim];
                    });
            }
            else
            {
                // Partition primitives using approximate SAH
                const unsigned int nBuckets = 12;
                BucketInfo buckets[nBuckets];

                // Initialize _BucketInfo_ for SAH partition buckets
                for (unsigned int i = start; i < end; ++i)
                {
                    int b = nBuckets * centroidBounds.Offset(primitiveInfo[i].centroid)[dim];
                    if (b == nBuckets) b = nBuckets - 1;
                    assert(b >= 0 && b < nBuckets);
                    buckets[b].count++;
                    buckets[b].bounds = Union(buckets[b].bounds, primitiveInfo[i].bounds);
                }

                // Compute costs for splitting after each bucket
                float cost[nBuckets - 1];
                for (unsigned int i = 0; i < nBuckets - 1; ++i)
                {
                    Bounds3 b0, b1;
                    int count0 = 0, count1 = 0;
                    for (unsigned int j = 0; j <= i; ++j)
                    {
                        b0 = Union(b0, buckets[j].bounds);
                        count0 += buckets[j].count;
                    }
                    for (unsigned int j = i + 1; j < nBuckets; ++j)
                    {
                        b1 = Union(b1, buckets[j].bounds);
                        count1 += buckets[j].count;
                    }
                    cost[i] = 1.0f + (count0 * b0.SurfaceArea() + count1 * b1.SurfaceArea()) / bounds.SurfaceArea();
                }

                // Find bucket to split at that minimizes SAH metric
                float minCost = cost[0];
                unsigned int minCostSplitBucket = 0;
                for (unsigned int i = 1; i < nBuckets - 1; ++i)
                {
                    if (cost[i] < minCost)
                    {
                        minCost = cost[i];
                        minCostSplitBucket = i;
                    }
                }

                // Either create leaf or split primitives at selected SAH bucket
                float leafCost = float(nPrimitives);
                if (nPrimitives > kMaxPrimitivesInNode || minCost < leafCost)
                {
                    BVHPrimitiveInfo* pmid = std::partition(&primitiveInfo[start], &primitiveInfo[end - 1] + 1,
                        [=](const BVHPrimitiveInfo& pi)
                        {
                            int b = nBuckets * centroidBounds.Offset(pi.centroid)[dim];
                            if (b == nBuckets) b = nBuckets - 1;
                            assert(b >= 0 && b < nBuckets);
                            return b <= minCostSplitBucket;
                        });
                    mid = pmid - &primitiveInfo[0];
                }
                else
                {
                    // Create leaf
                    unsigned int firstPrimOffset = orderedTriangles.size();
                    for (unsigned int i = start; i < end; ++i)
                    {
                        unsigned int primNum = primitiveInfo[i].primitiveNumber;
                        orderedTriangles.push_back(triangles[primNum]);
                    }
                    node->InitLeaf(firstPrimOffset, nPrimitives, bounds);

                    return node;
                }
            }

            node->InitInterior(dim,
                RecursiveBuild(triangles, primitiveInfo, start, mid,
                    totalNodes, orderedTriangles),
                RecursiveBuild(triangles, primitiveInfo, mid, end,
                    totalNodes, orderedTriangles));
        }
    }

    return node;
}

unsigned int Bvh::FlattenBVHTree(BVHBuildNode* node, unsigned int* offset)
{
    LinearBVHNode* linearNode = &nodes_[*offset];
    linearNode->bounds = node->bounds;
    unsigned int myOffset = (*offset)++;
    if (node->nPrimitives > 0)
    {
        assert(!node->children[0] && !node->children[1]);
        assert(node->nPrimitives < 65536);
        linearNode->offset = node->firstPrimOffset;
        linearNode->nPrimitives = node->nPrimitives;
    }
    else
    {
        // Create interior flattened BVH node
        linearNode->axis = node->splitAxis;
        linearNode->nPrimitives = 0;
        FlattenBVHTree(node->children[0], offset);
        linearNode->offset = FlattenBVHTree(node->children[1], offset);
    }

    return myOffset;
}

void Bvh::IntersectRays(cl::Buffer const& rays_buffer, std::uint32_t num_rays,
    cl::Buffer const& hits_buffer)
{
    intersect_kernel_->SetArgument(0, &rays_buffer, sizeof(rays_buffer));
    intersect_kernel_->SetArgument(1, &num_rays, sizeof(num_rays));
    intersect_kernel_->SetArgument(2, &triangles_buffer_, sizeof(triangles_buffer_));
    intersect_kernel_->SetArgument(3, &nodes_buffer_, sizeof(nodes_buffer_));
    intersect_kernel_->SetArgument(4, &hits_buffer, sizeof(hits_buffer));

    cl_context_.ExecuteKernel(*intersect_kernel_, num_rays);
}
