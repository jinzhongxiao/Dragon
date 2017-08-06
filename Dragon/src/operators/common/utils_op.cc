#include <algorithm>
#include "operators/common/utils_op.h"
#include "utils/math_functions.h"
#include "utils/op_kernel.h"

namespace dragon {

template <class Context> template <typename T>
void CopyOp<Context>::RunWithType() { 
    auto* Xdata = input(0).template data<T, Context>();
    auto* Ydata = output(0)->template mutable_data<T, Context>();
    ctx().template Copy<T, Context, Context>(output(0)->count(), Ydata, Xdata);
}

template <class Context>
void CopyOp<Context>::RunOnDevice() {
    output(0)->ReshapeLike(input(0));
    if (input(0).template IsType<float>()) RunWithType<float>();
    else if (input(0).template IsType<float16>()) RunWithType<float16>();
    else LOG(FATAL) << "unsupported input types.";
}

DEPLOY_CPU(Copy);
#ifdef WITH_CUDA
DEPLOY_CUDA(Copy);
#endif
OPERATOR_SCHEMA(Copy).NumInputs(1).NumOutputs(1);
NO_GRADIENT(Copy);

template <class Context> template <typename T>
void AccuracyOp<Context>::RunWithType() {
    if (OutputSize() > 1) {
        math::Set<T, CPUContext>(classes, 0, 
            output(1)->template mutable_data<T, CPUContext>());
    }
    Map<int, int> num_per_class;

    T acc = 0, count = 0;
    auto* Xdata = input(0).template data<T, CPUContext>();
    auto* labels = input(1).template data<T, CPUContext>();
    auto* ignores = ignore_labels.count() > 0 ?
                        ignore_labels.data<int, CPUContext>() : nullptr;
    const TIndex dim = input(0).count() / outer_num;
    for (int i = 0; i < outer_num; i++) {
        for (int j = 0; j < inner_num; j++) {
            const int label = labels[i * inner_num + j];
            for (int k = 0; k < ignore_labels.count(); k++)
                if (label == ignores[k]) continue;
            if (OutputSize() > 1) num_per_class[label]++;
            vector<pair<T, int> > vec;
            for (int k = 0; k < classes; k++)
                vec.push_back(std::make_pair(Xdata[i * dim + k * inner_num + j], k));
            std::partial_sort(vec.begin(), vec.begin() + top_k, vec.end(), std::greater<pair<T, int> >());
            for (int k = 0; k < top_k; k++) {
                if (vec[k].second == label) {
                    if (OutputSize() > 1)
                        output(1)->template mutable_data<T, CPUContext>()[label]++;
                    acc++;
                    break;
                }
            }
            count++;
        }    //  end inner_num
    }    // end outer_num

    output(0)->template mutable_data<T, CPUContext>()[0] = acc / count;
    if (OutputSize() > 1) {
        auto* acc_per_class = output(1)->template mutable_data<T, CPUContext>();
        for (int i = 0; i < classes; i++)
            acc_per_class[i] = num_per_class[i] == 0 ? 0 : acc_per_class[i] / acc_per_class[i];
    }
}

template <class Context>
void AccuracyOp<Context>::RunOnDevice() {
    outer_num = input(0).dim(0);
    inner_num = input(0).count(2);
    classes = input(0).dim(1);
    CHECK_EQ(outer_num * inner_num, input(1).count())
        << "\ngiven (" << outer_num << "," << inner_num << ") predictions"
        << "\nbut provided " << input(1).count() << " labels.";
    output(0)->Reshape(vector<TIndex>(1, 1));
    if (OutputSize() > 1) output(1)->Reshape(vector<TIndex>(1, classes)); 

    if (input(0).template IsType<float>()) RunWithType<float>();
    else LOG(FATAL) << "unsupported input types.";
}

DEPLOY_CPU(Accuracy);
#ifdef WITH_CUDA
DEPLOY_CUDA(Accuracy);
#endif
OPERATOR_SCHEMA(Accuracy).NumInputs(2).NumOutputs(1, 2);

NO_GRADIENT(Accuracy);

template <class Context> template <typename T>
void OneHotOp<Context>::RunWithType() {
    auto* Xdata = input(0).template data<T, Context>();
    auto* Ydata = output(0)->template mutable_data<T, Context>();
    math::Set<T, Context>(output(0)->count(), 
                          dragon_cast<T, float>(float(off_value)), 
                          Ydata);

    kernel::OneHot<T, Context>(input(0).count(), depth, on_value, Xdata, Ydata);
}

template <class Context>
void OneHotOp<Context>::RunOnDevice() {
    vector<TIndex> dims = input(0).dims();
    dims.push_back(depth);
    output(0)->Reshape(dims);
   
    if (input(0).template IsType<float>()) RunWithType<float>();
    else LOG(FATAL) << "unsupported input types.";
}

DEPLOY_CPU(OneHot);
#ifdef WITH_CUDA
DEPLOY_CUDA(OneHot);
#endif
OPERATOR_SCHEMA(OneHot).NumInputs(1).NumOutputs(1);

NO_GRADIENT(OneHot);



}    // namespace dragon
