#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <set>
#include <numeric>

static const size_t BIG_MEMORY_SIZE = size_t(1) << 30;
static const size_t MAX_ASSOC = 16;

char *cache_data;

#pragma optimize( "", off )
__attribute__ ((optimize(0))) int64_t measure_time(int stride, int s) {
  char **x;
  for (int i = (s - 1) * stride; i >= 0; i -= stride) {
    x = (char **) &cache_data[i];
    if (i >= stride) {
      x[0] = &cache_data[i - stride];
    } else {
      x[0] = &cache_data[(s - 1) * stride];
    }
  }

  const long long iter_count = 20;
  std::vector<long long> times;
  for (long long k = 0L; k < iter_count; k++) {
    auto start = std::chrono::high_resolution_clock::now();
    for (long long i = 0L; i < 1000 * 1000; i++) {
      x = (char **) *x;
    }
    auto end = std::chrono::high_resolution_clock::now();
    times.push_back((end - start).count());
  }
  return std::accumulate(times.begin(), times.end(), 0L) / iter_count;
}
#pragma optimize( "", on )

std::pair<std::vector<std::set<int>>, int> calculate_possible_cache_size() {
  std::vector<std::set<int>> all_possible_assocs;
  size_t possible_cache_size = MAX_ASSOC;

  for (; possible_cache_size < BIG_MEMORY_SIZE / MAX_ASSOC; possible_cache_size *= 2) {
    auto prev_time = measure_time(possible_cache_size, 1);
    std::set<int> possible_assocs;

    for (int assoc = 2; assoc <= MAX_ASSOC; assoc++) {
      auto curr_time = measure_time(possible_cache_size, assoc);
      if ((curr_time - prev_time) * 10 > curr_time) {
        possible_assocs.insert(assoc - 1);
      }
      prev_time = curr_time;
    }

    if (possible_cache_size >= 256 * 1024 && !all_possible_assocs.empty() && possible_assocs == all_possible_assocs.back()) {
      break;
    }

    all_possible_assocs.push_back(possible_assocs);
  }

  return {all_possible_assocs, possible_cache_size};
}

std::pair<int, int> infer_size_and_assoc(std::vector<std::set<int>> all_possible_assocs, int stride) {
  std::pair<int, int> min_cache{std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};

  std::reverse(all_possible_assocs.begin(), all_possible_assocs.end());
  std::set<int> jumps_to_process = all_possible_assocs[0];
  for (const auto &possible_assocs: all_possible_assocs) {
    std::set<int> jumps_to_delete;
    for (int assoc: jumps_to_process) {
      if (!possible_assocs.count(assoc)) {
        min_cache = std::min(min_cache, std::make_pair(stride * assoc, assoc));
        jumps_to_delete.insert(assoc);
      }
    }
    for (int s: jumps_to_delete) {
      jumps_to_process.erase(s);
    }
    stride /= 2;
  }
  return min_cache;
}

int infer_line_size(int cache_size, int cache_assoc) {
  int cache_line_size = -1;
  int prev_first_jump = 1025;

  for (int line_size = 1; line_size <= cache_size; line_size *= 2) {
    auto prev_time = measure_time(cache_size / cache_assoc + line_size, 2);
    int first_jump = -1;
    for (int S = 1; S <= 1024; S *= 2) {
      auto curr_time = measure_time(cache_size / cache_assoc + line_size, S + 1);
      if ((curr_time - prev_time) * 10 > curr_time && first_jump <= 0) {
        first_jump = S;
      }
      prev_time = curr_time;
    }
    if (first_jump > prev_first_jump) {
      return line_size * cache_assoc;
    }
    prev_first_jump = first_jump;
  }

  return cache_line_size;
}

int main() {
  std::cout << "Staring inference of L1 cache characteristics" << std::endl;

  cache_data = (char*) malloc(BIG_MEMORY_SIZE * sizeof(char));
  if (cache_data == nullptr) {
    std::cout << "Failed to allocate array";
    return 1;
  }
  auto [all_possible_assocs, possible_cache_size] = calculate_possible_cache_size();
  auto [cache_size, cache_assoc] = infer_size_and_assoc(all_possible_assocs, possible_cache_size);

  int cache_line_size = infer_line_size(cache_size, cache_assoc);

  std::cout << "Inferred L1 cache characteristics:" << std::endl
            << "Size = " << cache_size << std::endl
            << "Associativity = " << cache_assoc << std::endl
            << "Line size = " << cache_line_size << std::endl;

  free(cache_data);
  return 0;
}
