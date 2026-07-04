#include "heap.h"
#include "debug.h"
#include "matrix.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

/**************************矩阵的运算***************************/

/*
**@breif    生成单位矩阵
**@param    单位矩阵输出
**@param    (n * n)维度
**@retval   None
*/

static void matrix_identity(float *matrix_i, int n)
{
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            matrix_i[i * n + j] = (i == j ? 1 : 0);
        }
    }
}

/*
**@breif    矩阵乘法(A * B)  【A、B要满足A：m * n  ，  B：n * p】
**@param    输入矩阵1
**@param    矩阵1行数  m * n
**@param    矩阵1列数
**@param    输入矩阵2  n * p
**@param    矩阵2列数
**@param    输出矩阵
**@retval   None
*/

static void matrix_multiply(float *matrix1, int m, int n, float *matrix2, int p, float *matrix_out)
{
    for (int i = 0; i < m; i++)
    {
        for (int j = 0; j < p; j++)
        {
            matrix_out[i * p + j] = 0;
            for (int k = 0; k < n; k++)
            {
                matrix_out[i * p + j] += matrix1[i * n + k] * matrix2[k * p + j];
            }
        }
    }
}

/*
**@breif    矩阵加法(A + B)  【A、B要满足行列均等】
**@param    输入矩阵1
**@param    输入矩阵2
**@param    输入矩阵行数
**@param    输入矩阵列数
**@param    输出矩阵
**@retval   None
*/

static void matrix_add(float *matrix1, float *matrix2, int m, int n, float *matrix_out)
{
    for (int i = 0; i < m * n; i++)
    {
        matrix_out[i] = matrix1[i] + matrix2[i];
    }
}

/*
**@breif    矩阵减法(A - B)  【A、B要满足行列均等】
**@param    输入矩阵1
**@param    输入矩阵2
**@param    输入矩阵行数
**@param    输入矩阵列数
**@param    输出矩阵
**@retval   None
*/

static void matrix_reduce(float *matrix1, float *matrix2, int m, int n, float *matrix_out)
{
    for (int i = 0; i < m * n; i++)
    {
        matrix_out[i] = matrix1[i] - matrix2[i];
    }
}

/*
**@breif    矩阵转置
**@param    输入矩阵
**@param    输入矩阵行数
**@param    输入矩阵列数
**@param    输出矩阵
**@retval   None
*/

static void matrix_transpose(float *matrix, int m, int n, float *matrix_transe)
{
    for (int i = 0; i < m; i++)
    {
        for (int j = 0; j < n; j++)
        {
            matrix_transe[j * m + i] = matrix[i * n + j];
        }
    }
}

/*
**@breif    矩阵求逆【LU分解，此方法求逆精度很高】
**@param    (n * n)方阵维度
**@param    输出矩阵
**@retval   1--矩阵非奇异
*/

static uint8_t matrix_inverse_lu(float *matrix, int n, float *matrix_inv)
{
    // float *L = new float[n * n];     // 下三角矩阵
    // float *U = new float[n * n];     // 上三角矩阵
    // float *L_inv = new float[n * n]; // 下三角矩阵逆
    float *L = (float *)(heap.api->malloc(heap.ctrl, n * n * sizeof(float)));
    float *U = (float *)(heap.api->malloc(heap.ctrl, n * n * sizeof(float)));
    float *L_inv = (float *)(heap.api->malloc(heap.ctrl, n * n * sizeof(float)));

    if (L == NULL || U == NULL || L_inv == NULL)
    {
        printf("Inverse_LU heap error\n");
        return 0;
    }

    matrix_identity(L, n); // 初始化为单位矩阵
    memcpy(U, matrix, n * n * sizeof(float));

    // 高斯消元(默认对角线元素为主元)
    for (int j = 0; j < n - 1; j++) // 需要消元的轮数
    {
        for (int i = j + 1; i < n; i++) // 行遍历
        {
            L[i * n + j] = U[i * n + j] / U[j * n + j]; // 存储消元因子
            for (int k = j; k < n; k++)                 // 列遍历
            {
                U[i * n + k] -= L[i * n + j] * U[j * n + k]; // 消元
            }
        }
    }

    // 求逆(通过L * L_inv = I和U * A_Inv = L_Inv可求得A_inv)
    double sum = 0;
    for (int j = 0; j < n; j++) // 下三角，向前替换求逆
    {
        for (int i = 0; i < n; i++)
        {
            sum = 0;
            for (int k = 0; k < i; k++)
            {
                sum += L[i * n + k] * L_inv[k * n + j];
            }
            if (j == i)
                L_inv[i * n + j] = 1 - sum;
            else
                L_inv[i * n + j] = -sum;
        }
    }

    for (int j = 0; j < n; j++) // 上三角，向后替换求逆
    {
        for (int i = n - 1; i >= 0; i--)
        {
            sum = 0;
            for (int k = i + 1; k < n; k++)
            {
                sum += U[i * n + k] * matrix_inv[k * n + j];
            }
            if (fabs(U[i * n + i]) < 1e-6) // U接近奇异
            {
                printf("matrix -> 0\n");
                heap.api->free(heap.ctrl, L);
                heap.api->free(heap.ctrl, U);
                heap.api->free(heap.ctrl, L_inv);

                return 0;
            }
            matrix_inv[i * n + j] = (L_inv[i * n + j] - sum) / U[i * n + i];
        }
    }

    heap.api->free(heap.ctrl, L);
    heap.api->free(heap.ctrl, U);
    heap.api->free(heap.ctrl, L_inv);

    return 1;
}

/*
**@breif    高斯消元法求矩阵行列式
**@param    输入矩阵
**@param    (n * n)方阵维度
**@retval   行列式结果
*/

static float matrix_gauss_deter(float *matrix, int n)
{
    int i, j, k, max_row, swap_num = 0;
    double factor, det = 1.0;

    for (j = 0; j < n - 1; j++) // 需要消元的轮数
    {
        max_row = j;                // 假设当前j行为主元(首先选对角线元素)
        for (i = j + 1; i < n; i++) // 行遍历
        {
            if (fabs(matrix[i * n + j]) > fabs(matrix[max_row * n + j]))
                max_row = i; // 选绝对值最大为主元
        }
        if (fabs(matrix[max_row * n + j]) < 1e-6) // 主元为接近0
            return 0;

        // 交换行
        if (max_row != j)
        {
            for (k = 0; k < n; k++) // 列遍历
            {
                double temp = matrix[j * n + k];
                matrix[j * n + k] = matrix[max_row * n + k];
                matrix[max_row * n + k] = temp;
            }
            swap_num++;
        }

        // 消元
        for (i = j + 1; i < n; i++) // 行遍历
        {
            factor = matrix[i * n + j] / matrix[j * n + j]; // 消元因子
            for (k = j; k < n; k++)                         // 列遍历
            {
                matrix[i * n + k] -= factor * matrix[j * n + k];
            }
        }
    }

    // 计算行列式的值
    for (i = 0; i < n; i++)
    {
        if (fabs(matrix[i * n + i]) < 1e-6)
            return 0;
        det *= matrix[i * n + i];
    }
    if (swap_num % 2 == 1)
        det = -det;

    return det;
}

/*
**@breif    高斯消元【用于线性方程求解】
**@param    原矩阵(n * n)
**@param    系数矩阵(n * 1)
**@param    原矩阵列数
**@param    线性方程的解
**@retval   1--矩阵非奇异
*/

static uint8_t matrix_gauss_elimination(float *matrix_a, float *matrix_b, int n, float *solution)
{
    int i, j, k, max_row;
    double factor;
    // double为8字节大小
    // 注：默认堆空间大小是0x200大小(512byte)，可在启动文件页面下方的Configuration Wizard更改堆区(Heap)大小【栈区(Stack)也可以更改】

    // 增广矩阵
    float *matrix_aug = heap.api->malloc(heap.ctrl, (n * (n + 1) * sizeof(float)));

    if (matrix_aug == NULL)
    {
        // Serial_Printf("Error：%d\n", n * (n + 1));
        return 0;
    }
    for (i = 0; i < n; i++)
    {
        for (j = 0; j < n; j++)
        {
            matrix_aug[i * (n + 1) + j] = matrix_a[i * n + j];
        }
        matrix_aug[i * (n + 1) + j] = matrix_b[i];
    }

    for (j = 0; j < n - 1; j++) // 列遍历
    {
        max_row = j;                // 假设当前j列为主元(首先选对角线元素)
        for (i = j + 1; i < n; i++) // 行遍历
        {
            if (fabs(matrix_aug[i * (n + 1) + j]) > fabs(matrix_aug[max_row * (n + 1) + j]))
                max_row = i; // 选绝对值最大为主元
        }

        if (fabs(matrix_aug[max_row * (n + 1) + j]) < 1e-6) // 主元接近0时，矩阵奇异
        {
            heap.api->free(heap.ctrl, matrix_aug);
            return 0;
        }

        // 交换行
        if (max_row != j)
        {
            for (k = 0; k < n + 1; k++) // 列遍历
            {
                double temp = matrix_aug[j * (n + 1) + k];
                matrix_aug[j * (n + 1) + k] = matrix_aug[max_row * (n + 1) + k];
                matrix_aug[max_row * (n + 1) + k] = temp;
            }
        }

        // 消元
        for (i = j + 1; i < n; i++) // 行遍历
        {
            factor = matrix_aug[i * (n + 1) + j] / matrix_aug[j * (n + 1) + j]; // 消元因子
            for (k = j; k < n + 1; k++)                                         // 列遍历
            {
                matrix_aug[i * (n + 1) + k] -= factor * matrix_aug[j * (n + 1) + k];
            }
        }
    }

    // 回代求解
    for (j = n - 1; j >= 0; j--)
    {
        solution[j] = matrix_aug[j * (n + 1) + n];
        for (i = j + 1; i < n; i++)
        {
            solution[j] -= matrix_aug[j * (n + 1) + i] * solution[i];
        }
        solution[j] /= matrix_aug[j * (n + 1) + j];
    }

    heap.api->free(heap.ctrl, matrix_aug);
    return 1;
}

/*
**@breif    雅可比旋转分离特征值
**@param    正定矩阵(对角线对称)
**@param    (n * n)方阵维度
**@param    正交矩阵
**@param    特征值矩阵(n * n)
**@param    迭代最大次数
**@retval   None
*/

static void matrix_jacobi(float *matrix_pos, int n, float *matrix_orth, float *diag, int iter_max)
{
    // 初始化Matrix_Orth为单位矩阵
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            matrix_orth[i * n + j] = (i == j) ? 1.0 : 0.0;
        }
    }

    for (int iter = 0; iter < iter_max; iter++) // 迭代次数
    {
        // 寻找最大非对角元素
        double max = 0.0;
        int p, q;
        for (int i = 0; i < n; i++)
        {
            for (int j = i + 1; j < n; j++)
            {
                if (fabs(matrix_pos[i * n + j]) > max)
                {
                    max = fabs(matrix_pos[i * n + j]);
                    p = i;
                    q = j;
                }
            }
        }

        if (max < 1e-12)
            break; // 收敛

        // 计算旋转角度(旋转矩阵元素)
        double theta = 0.5 * atan2(2 * matrix_pos[p * n + q], matrix_pos[q * n + q] - matrix_pos[p * n + p]);
        double c = cos(theta);
        double s = sin(theta);

        // 更新Matrix_Pos矩阵
        double Pos_pp = c * c * matrix_pos[p * n + p] - 2 * s * c * matrix_pos[p * n + q] + s * s * matrix_pos[q * n + q];
        double Pos_qq = s * s * matrix_pos[p * n + p] + 2 * s * c * matrix_pos[p * n + q] + c * c * matrix_pos[q * n + q];
        // double Pos_pq = (c * c - s * s) * matrix_pos[p * n + q] + s * c * (matrix_pos[p * n + p] - matrix_pos[q * n + q]);

        matrix_pos[p * n + p] = Pos_pp;
        matrix_pos[q * n + q] = Pos_qq;
        matrix_pos[p * n + q] = matrix_pos[q * n + p] = 0.0;

        for (int j = 0; j < n; j++)
        {
            if (j != p && j != q)
            {
                double Pos_pj = c * matrix_pos[p * n + j] - s * matrix_pos[q * n + j];
                double Pos_qj = s * matrix_pos[p * n + j] + c * matrix_pos[q * n + j];
                matrix_pos[p * n + j] = matrix_pos[j * n + p] = Pos_pj;
                matrix_pos[q * n + j] = matrix_pos[j * n + q] = Pos_qj;
            }
        }

        // 更新Matrix_Orth矩阵(累积旋转) -> 特征向量
        for (int i = 0; i < n; i++)
        {
            double orth_ip = matrix_orth[i * n + p];
            double orth_iq = matrix_orth[i * n + q];
            matrix_orth[i * n + p] = c * orth_ip - s * orth_iq;
            matrix_orth[i * n + q] = s * orth_ip + c * orth_iq;
        }
    }

    // 提取特征值
    for (int i = 0; i < n; i++)
    {
        diag[i] = matrix_pos[i * n + i];
    }
}

/*
**@breif    打印矩阵
**@param    输入矩阵
**@param    输入矩阵行数
**@param    输入矩阵列数
**@retval   None
*/

static void matrix_printf(float *matrix, int m, int n)
{
    for (int i = 0; i < m; i++)
    {
        for (int j = 0; j < n; j++)
        {
            printf("%f ", matrix[i * n + j]);
        }
        printf("\n");
        printf("%d-----------------\n", i);
    }
}

//////////////////////////////////////////////////////////
const MATRIX_API_t matrix_api = {
    .identity = matrix_identity,
    .multiply = matrix_multiply,
    .add = matrix_add,
    .reduce = matrix_reduce,
    .transpose = matrix_transpose,
    .inverse_lu = matrix_inverse_lu,
    .gauss_deter = matrix_gauss_deter,
    .gauss_elimination = matrix_gauss_elimination,
    .jacobi = matrix_jacobi,
    .printf = matrix_printf,
};
