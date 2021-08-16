#ifndef TIRO_SPEECH_SRC_ITN_MAXMIN_WEIGHT_H_
#define TIRO_SPEECH_SRC_ITN_MAXMIN_WEIGHT_H_

#include <fst/float-weight.h>
#include <fst/util.h>
#include <fst/weight.h>

#include <string>

namespace fst {
template <class T>
class MaxMinWeightTpl : public FloatWeightTpl<T> {
 public:
  using typename FloatWeightTpl<T>::ValueType;
  using FloatWeightTpl<T>::Value;
  using ReverseWeight = MaxMinWeightTpl<T>;
  using Limits = FloatLimits<T>;

  MaxMinWeightTpl() : FloatWeightTpl<T>() {}

  MaxMinWeightTpl(T f) : FloatWeightTpl<T>(f) {}

  MaxMinWeightTpl(const MaxMinWeightTpl<T> &weight)
      : FloatWeightTpl<T>(weight) {}

  static const MaxMinWeightTpl &Zero() {
    static const MaxMinWeightTpl zero(Limits::NegInfinity());
    return zero;
  }

  static const MaxMinWeightTpl &One() {
    static const MaxMinWeightTpl one(Limits::PosInfinity());
    return one;
  }

  static const MaxMinWeightTpl &NoWeight() {
    static const MaxMinWeightTpl no_weight(Limits::NumberBad());
    return no_weight;
  }

  static const std::string &Type() {
    static const std::string *const type = new std::string(
        std::string("maxmin") + FloatWeightTpl<T>::GetPrecisionString());
    return *type;
  }

  // Fails for IEEE NaN.
  bool Member() const { return Value() == Value(); }

  MaxMinWeightTpl<T> Quantize(float delta = kDelta) const {
    // If one of infinities, or a NaN.
    if (!Member() || Value() == Limits::NegInfinity() ||
        Value() == Limits::PosInfinity()) {
      return *this;
    } else {
      return MaxMinWeightTpl<T>(floor(Value() / delta + 0.5F) * delta);
    }
  }

  MaxMinWeightTpl<T> Reverse() const { return *this; }

  static constexpr uint64 Properties() {
    return kLeftSemiring | kRightSemiring | kCommutative | kIdempotent | kPath;
  }
};

// Single-precision min-max weight.
using MaxMinWeight = MaxMinWeightTpl<float>;

template <class T>
inline MaxMinWeightTpl<T> Plus(const MaxMinWeightTpl<T> &w1,
                               const MaxMinWeightTpl<T> &w2) {
  if (!w1.Member() || !w2.Member()) return MaxMinWeightTpl<T>::NoWeight();
  return w1.Value() > w2.Value() ? w1 : w2;
}

inline MaxMinWeightTpl<float> Plus(const MaxMinWeightTpl<float> &w1,
                                   const MaxMinWeightTpl<float> &w2) {
  return Plus<float>(w1, w2);
}

inline MaxMinWeightTpl<double> Plus(const MaxMinWeightTpl<double> &w1,
                                    const MaxMinWeightTpl<double> &w2) {
  return Plus<double>(w1, w2);
}

// Min.
template <class T>
inline MaxMinWeightTpl<T> Times(const MaxMinWeightTpl<T> &w1,
                                const MaxMinWeightTpl<T> &w2) {
  if (!w1.Member() || !w2.Member()) return MaxMinWeightTpl<T>::NoWeight();
  return w1.Value() <= w2.Value() ? w1 : w2;
}

inline MaxMinWeightTpl<float> Times(const MaxMinWeightTpl<float> &w1,
                                    const MaxMinWeightTpl<float> &w2) {
  return Times<float>(w1, w2);
}

inline MaxMinWeightTpl<double> Times(const MaxMinWeightTpl<double> &w1,
                                     const MaxMinWeightTpl<double> &w2) {
  return Times<double>(w1, w2);
}

// Defined only for special cases.
template <class T>
inline MaxMinWeightTpl<T> Divide(const MaxMinWeightTpl<T> &w1,
                                 const MaxMinWeightTpl<T> &w2,
                                 DivideType /* typ = DIVIDE_ANY */) {
  if (!w1.Member() || !w2.Member()) return MaxMinWeightTpl<T>::NoWeight();
  // min(w1, x) = w2, w1 >= w2 => min(w1, x) = w2, x = w2.
  return w1.Value() <= w2.Value()
             ? w1
             : MaxMinWeightTpl<T>{FloatLimits<T>::NumberBad()};
}

inline MaxMinWeightTpl<float> Divide(const MaxMinWeightTpl<float> &w1,
                                     const MaxMinWeightTpl<float> &w2,
                                     DivideType typ = DIVIDE_ANY) {
  return Divide<float>(w1, w2, typ);
}

inline MaxMinWeightTpl<double> Divide(const MaxMinWeightTpl<double> &w1,
                                      const MaxMinWeightTpl<double> &w2,
                                      DivideType typ = DIVIDE_ANY) {
  return Divide<double>(w1, w2, typ);
}

// This function object returns random integers chosen from [0,
// num_random_weights). The boolean 'allow_zero' determines whether Zero() and
// zero divisors should be returned in the random weight generation. This is
// intended primary for testing.
template <class T>
class WeightGenerate<MaxMinWeightTpl<T>> {
 public:
  using Weight = MaxMinWeightTpl<T>;

  explicit WeightGenerate(bool allow_zero = true,
                          size_t num_random_weights = kNumRandomWeights)
      : allow_zero_(allow_zero), num_random_weights_(num_random_weights) {}

  Weight operator()() const {
    const int n = (rand() %  // NOLINT
                   (2 * num_random_weights_ + allow_zero_)) -
                  num_random_weights_;
    if (allow_zero_ && n == num_random_weights_) {
      return Weight::Zero();
    } else if (n == -num_random_weights_) {
      return Weight::One();
    } else {
      return Weight(n);
    }
  }

 private:
  // Permits Zero() and zero divisors.
  const bool allow_zero_;
  // Number of alternative random weights.
  const size_t num_random_weights_;
};

}  // end namespace fst

#endif  // TIRO_SPEECH_SRC_ITN_MAXMIN_WEIGHT_H_
