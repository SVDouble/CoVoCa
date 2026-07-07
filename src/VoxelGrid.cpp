#include <filesystem>
#include <fstream>

#include "VoxelGrid.h"

VoxelGrid::VoxelGrid(Eigen::Vector3d _bounding_box_min,
                     Eigen::Vector3d _bounding_box_max,
                     Eigen::Vector3i _size)
    : m_size(_size),
      m_center((_bounding_box_min + _bounding_box_max) * 0.5)
{
    int nx = _size.x();
    int ny = _size.y();
    int nz = _size.z();

    m_voxel_flattened_matrix.resize(nx * ny * nz);

    Eigen::Vector3d dimensions = _bounding_box_max - _bounding_box_min;

    m_step_size.x() = dimensions.x() / (nx - 1);
    m_step_size.y() = dimensions.y() / (ny - 1);
    m_step_size.z() = dimensions.z() / (nz - 1);

    for (int z = 0; z < nz; ++z)
        for (int y = 0; y < ny; ++y)
            for (int x = 0; x < nx; ++x)
            {
                int index = x + nx * (y + ny * z);

                Eigen::Vector3d position;
                position.x() = _bounding_box_min.x() + x * m_step_size.x();
                position.y() = _bounding_box_min.y() + y * m_step_size.y();
                position.z() = _bounding_box_min.z() + z * m_step_size.z();

                m_voxel_flattened_matrix[index] =
                    Voxel(position, Eigen::Vector3i(x, y, z));
            }
}

Eigen::Vector3i VoxelGrid::getSize() const
{
    return m_size;
}

const std::vector<Voxel> &VoxelGrid::getVoxelGrid() const
{
    return m_voxel_flattened_matrix;
}

Eigen::Vector3d VoxelGrid::getStepSize() const
{
    return m_step_size;
}

const Voxel &VoxelGrid::getVoxel(int _row, int _column, int _depth) const
{
    return m_voxel_flattened_matrix[calculateFlattenedIndex(m_size, _row, _column, _depth)];
}

void VoxelGrid::setVoxelOccupied(bool _occupied, int _index)
{
    m_voxel_flattened_matrix[_index].setOccupied(_occupied);
}

void VoxelGrid::setVoxelColor(const Eigen::Vector3i &_color, int _index)
{
    m_voxel_flattened_matrix[_index].setColor(_color);
}

void VoxelGrid::saveVoxelGrid()
{
    saveVoxelGrid("voxel_grid.ply");
}

void VoxelGrid::saveVoxelGrid(const std::filesystem::path &_path)
{
    if (_path.has_parent_path())
    {
        std::filesystem::create_directories(_path.parent_path());
    }

    std::ofstream file(_path);

    if (!file.is_open())
    {
        std::cerr << "Could not create " << _path.string() << "\n";
        return;
    }

    // Count occupied voxels for file header
    int occupied_count = 0;

    for (const auto &voxel : m_voxel_flattened_matrix)
    {
        if (voxel.getOccupied())
        {
            occupied_count++;
        }
    }

    // Header
    file << "ply\n"; // PLY format
    file << "format ascii 1.0\n";
    file << "element vertex " << occupied_count << "\n"; // number of vertexes
    file << "property float x\n";                        // vertexes defined by x,y,z position
    file << "property float y\n";
    file << "property float z\n";
    file << "property uchar red\n"; // RGB color properties
    file << "property uchar green\n";
    file << "property uchar blue\n";
    file << "end_header\n";

    // Write voxel centers
    for (const auto &voxel : m_voxel_flattened_matrix)
    {
        if (!voxel.getOccupied())
            continue;

        Eigen::Vector3d pos = voxel.getCartesianPos();
        Eigen::Vector3i color = voxel.getColor();

        file << pos.x() << " "
             << pos.y() << " "
             << pos.z() << " "
             << color.x() << " "
             << color.y() << " "
             << color.z() << "\n";
    }

    file.close();

    std::cout << "Saved " << occupied_count
              << " occupied voxels to " << _path.string() << "\n";
}

bool VoxelGrid::isOccupied(int x, int y, int z) const
{
    // Outside grid always unoccupied

    if (x < 0 || x >= m_size.x() ||
        y < 0 || y >= m_size.y() ||
        z < 0 || z >= m_size.z())
    {
        return false;
    }

    int index = x + y * m_size.x() + z * m_size.x() * m_size.y();

    return m_voxel_flattened_matrix[index].getOccupied();
}

void VoxelGrid::saveHullMesh()
{
    saveHullMesh("voxel_hull.ply");
}

void VoxelGrid::saveHullMesh(const std::filesystem::path &_path)
{
    if (_path.has_parent_path())
    {
        std::filesystem::create_directories(_path.parent_path());
    }

    std::ofstream file(_path);

    if (!file.is_open())
    {
        std::cerr << "Cannot create " << _path.string() << "\n";
        return;
    }

    // Containers
    std::vector<Eigen::Vector3d> vertices;
    std::vector<Eigen::Vector3i> vertexColors;
    std::vector<Eigen::Vector3i> triangles;

    // Neighbor directions
    const Eigen::Vector3i directions[6] =
        {
            {-1, 0, 0},
            {1, 0, 0},
            {0, -1, 0},
            {0, 1, 0},
            {0, 0, -1},
            {0, 0, 1}};

    // Face definitions
    const int faceCorners[6][4] =
        {
            {0, 3, 7, 4}, // -X
            {1, 5, 6, 2}, // +X
            {0, 4, 5, 1}, // -Y
            {3, 2, 6, 7}, // +Y
            {0, 1, 2, 3}, // -Z
            {4, 7, 6, 5}  // +Z
        };

    // Iterate over all voxels
    for (int z = 0; z < m_size.z(); z++)
    {
        for (int y = 0; y < m_size.y(); y++)
        {
            for (int x = 0; x < m_size.x(); x++)
            {
                if (!isOccupied(x, y, z))
                {
                    continue;
                }

                int index = x + y * m_size.x() + z * m_size.x() * m_size.y();

                const Voxel &voxel = m_voxel_flattened_matrix[index];

                Eigen::Vector3d center = voxel.getCartesianPos();
                Eigen::Vector3i color = voxel.getColor();

                Eigen::Vector3d half = m_step_size / 2.0;

                std::vector<Eigen::Vector3d> corners(8);

                corners[0] = center + Eigen::Vector3d(-half.x(), -half.y(), -half.z());
                corners[1] = center + Eigen::Vector3d(half.x(), -half.y(), -half.z());
                corners[2] = center + Eigen::Vector3d(half.x(), half.y(), -half.z());
                corners[3] = center + Eigen::Vector3d(-half.x(), half.y(), -half.z());

                corners[4] = center + Eigen::Vector3d(-half.x(), -half.y(), half.z());
                corners[5] = center + Eigen::Vector3d(half.x(), -half.y(), half.z());
                corners[6] = center + Eigen::Vector3d(half.x(), half.y(), half.z());
                corners[7] = center + Eigen::Vector3d(-half.x(), half.y(), half.z());

                // Check all six neighbors
                for (int dir = 0; dir < 6; dir++)
                {
                    int nx = x + directions[dir].x();
                    int ny = y + directions[dir].y();
                    int nz = z + directions[dir].z();

                    // Skip interior faces
                    if (isOccupied(nx, ny, nz))
                    {
                        continue;
                    }

                    int c0 = faceCorners[dir][0];
                    int c1 = faceCorners[dir][1];
                    int c2 = faceCorners[dir][2];
                    int c3 = faceCorners[dir][3];

                    int start = static_cast<int>(vertices.size());

                    vertices.push_back(corners[c0]);
                    vertexColors.push_back(color);

                    vertices.push_back(corners[c1]);
                    vertexColors.push_back(color);

                    vertices.push_back(corners[c2]);
                    vertexColors.push_back(color);

                    vertices.push_back(corners[c3]);
                    vertexColors.push_back(color);

                    triangles.emplace_back(start, start + 1, start + 2);
                    triangles.emplace_back(start, start + 2, start + 3);
                }
            }
        }
    }

    // PLY header
    file << "ply\n";
    file << "format ascii 1.0\n";

    file << "element vertex " << vertices.size() << "\n";
    file << "property float x\n";
    file << "property float y\n";
    file << "property float z\n";
    file << "property uchar red\n";
    file << "property uchar green\n";
    file << "property uchar blue\n";

    file << "element face " << triangles.size() << "\n";
    file << "property list uchar int vertex_indices\n";
    file << "end_header\n";

    // Write vertices and colors
    for (size_t i = 0; i < vertices.size(); i++)
    {
        const auto &v = vertices[i];
        const auto &c = vertexColors[i];

        file << v.x() << " "
             << v.y() << " "
             << v.z() << " "
             << c.x() << " "
             << c.y() << " "
             << c.z() << "\n";
    }

    // Write triangles
    for (const auto &t : triangles)
    {
        file << "3 "
             << t.x() << " "
             << t.y() << " "
             << t.z() << "\n";
    }

    file.close();

    std::cout << "Saved "
              << triangles.size()
              << " triangles to " << _path.string() << "\n";
}
