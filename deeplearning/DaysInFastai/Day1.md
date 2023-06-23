> Inspired by [Why you (yes, you) should blog](https://medium.com/@racheltho/why-you-yes-you-should-blog-7d2544ac1045)

这几天晚上时间比较多，看到知乎上有人推荐 [Deep Learning for Coders with fastai and PyTorch](https://learning.oreilly.com/library/view/deep-learning-for/9781492045519/), 说是
高中数学就可以学会的 Deep Learning. M 公司的福利之一就是可以随便看 oreilly 的书，所以我也去学习了这本书，看了俩章，受书中所提 "Why you (yes, you) should blog" 感染，试着写点自己的感受，
以飨自己未来观看之用，也希望对别人有点帮助。

# Deep learning != Difficult Math

书中作者多次 diss 学院派，说他们给初学者一下子就是各种高深的数学公式，吓唬学生。我去查了一下作者的来头，嗯嗯，果然有一定分量，fastai 的初始研究员。
所以他在第一章不遗余力的鼓励读者，不要怕，跟着我学，高中数学也能学会。。。

他为了鼓励读者，还举了一个例子，大概是有个令初学者难以企及的高山，在老师讲完他的原理之后，学生惊奇的问："Is it that so?" 原理真的这么简单吗？很期待看看这个高山是什么，原理是什么。

他也讲了这本书的结构，是由浅入深，先是 fastai library 的使用（十行代码，实现 cat/dog 的分类器！普通游戏本只需一分钟的训练时间！），后面还会讲到 fastai API 背后的原理是什么，怎么用 PyTorch,
以及怎么看论文，实现论文。

他这种由浅入深的讲述方式值得学习，一下子就进入细节容易盲人摸象，看不到整体，先有个整体的概念，了解每一块大致做什么的，再逐一了解每一块具体做什么，细节是什么，挺好的。

# Image Recognition > Image Recognition

图像识别只能用于图像吗？他这里面举到一个例子，他的学生把声音识别通过频率等参数，绘制成图像
![image](https://github.com/Leteno/blog/assets/25839908/97aa9d52-6e97-433a-83d1-e4d398b52acc)
再走图像识别，去做环境音识别，居然[打败了当时最先进的环境音识别算法](https://etown.medium.com/great-results-on-audio-classification-with-fastai-library-ccaf906c5f52).

挺有意思的，以后有类似的问题，或许可以先通过某种算法生成图片，肉眼看一眼是不是有一定的辨识度，如果是，上图像识别可能有不错的结果。

# HuggingFace: connect Engineer and Researcher

工程师比较关心的是 API 调用，研究员关心的是 Model。可否有桥梁把俩者结合？他的配套视频里面讲到了 HuggingFace 网站。简单来说，Researcher 可以在上面放自己的 model，或者是代码，构建出 API。
工程师可以通过网络 API 调用的方式，调用其 model，拿到数据，再到自己的应用上表达出来。

TODO: 放置一个 API 在 HuggingFace 上，然后再本地调用。期间，关心费率，调用效率等参数。

# 结语

深度学习，之前一直觉得无法企及的高山，正在慢慢地褪去面纱，一位登山者，满怀敬意与敬畏之心，虔诚地跟在师傅后面，慢慢往上面爬。
