#pragma once

#include <iostream>
#include <Eigen/Dense>

int calculateFlattenedIndex(int _size, int _row, int _coloumn, int _depth)
{
    return _row + _size * (_coloumn + _size * _depth);
}