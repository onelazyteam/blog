#include <iostream>
#include <vector>
#include <cmath>
#include <limits>
#include <algorithm>
#include <random>

using namespace std;

typedef vector<double> Point;

// 计算两个点之间的欧式距离平方
double squared_distance(const Point& a, const Point& b) {
    double dist = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        dist += (a[i] - b[i]) * (a[i] - b[i]);
    }
    return dist;
}

class KMeans {
public:
    int K; // 目标簇的数量
    int max_iterations;  // 最大迭代次数
    vector<Point> centroids;

    KMeans(int k, int iterations = 100) : K(k), max_iterations(iterations) {}

    // 1. 传统的随机初始化
    void initialize_random(const vector<Point>& data) {
        centroids.clear();
        vector<Point> temp_data = data;
        auto rng = default_random_engine(random_device{}());
        shuffle(temp_data.begin(), temp_data.end(), rng);
        for (int i = 0; i < K; ++i) {
            centroids.push_back(temp_data[i]);
        }
    }

    // 2. K-Means++ 初始化 (核心改进)
    void initialize_plus_plus(const vector<Point>& data) {
        centroids.clear();
        auto rng = default_random_engine(random_device{}());
        uniform_int_distribution<int> dist_idx(0, data.size() - 1);

        // 第一步：随机选第一个中心
        centroids.push_back(data[dist_idx(rng)]);

        // 第二步：循环选出剩下的 K-1 个中心
        while (centroids.size() < (size_t)K) {
            vector<double> distances_sq;
            double sum_dist_sq = 0;

            for (const auto& p : data) {
                double min_dist_sq = numeric_limits<double>::max();
                // 找到该点到已有中心的最短距离
                for (const auto& c : centroids) {
                    min_dist_sq = min(min_dist_sq, squared_distance(p, c));
                }
                distances_sq.push_back(min_dist_sq);
                sum_dist_sq += min_dist_sq;
            }

            // 轮盘赌选择：距离越大的点，选中的概率越高
            discrete_distribution<int> weighted_dist(distances_sq.begin(), distances_sq.end());
            centroids.push_back(data[weighted_dist(rng)]);
        }
    }

    // 运行聚类
    void fit(const vector<Point>& data, bool use_plus_plus = true) {
        if (use_plus_plus) initialize_plus_plus(data);
        else initialize_random(data);

        for (int iter = 0; iter < max_iterations; ++iter) {
            vector<vector<Point>> clusters(K);
            
            // 分配阶段
            for (const auto& p : data) {
                int best_cluster = 0;
                double min_dist = numeric_limits<double>::max();
                for (int i = 0; i < K; ++i) {
                    double d = squared_distance(p, centroids[i]);
                    if (d < min_dist) {
                        min_dist = d;
                        best_cluster = i;
                    }
                }
                clusters[best_cluster].push_back(p);
            }

            // 更新阶段
            vector<Point> new_centroids = centroids;
            bool changed = false;
            for (int i = 0; i < K; ++i) {
                if (clusters[i].empty()) continue;
                
                Point mean(data[0].size(), 0.0);
                for (const auto& p : clusters[i]) {
                    for (size_t j = 0; j < p.size(); ++j) mean[j] += p[j];
                }
                for (double& val : mean) val /= clusters[i].size();

                if (squared_distance(mean, centroids[i]) > 1e-6) {
                    new_centroids[i] = mean;
                    changed = true;
                }
            }

            centroids = new_centroids;
            if (!changed) break; // 已收敛
        }
    }
};

int main() {
    // 测试数据
    vector<Point> data = {{1, 1}, {1.5, 2}, {3, 4}, {5, 7}, {3.5, 5}, {4.5, 5}, {10, 10}, {10, 11}};

    KMeans km(2);
    km.fit(data, true); // 使用 K-Means++

    cout << "最终聚类中心: " << endl;
    for (const auto& c : km.centroids) {
        cout << "(" << c[0] << ", " << c[1] << ")" << endl;
    }

    return 0;
}
