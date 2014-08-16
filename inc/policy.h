// Learning inference policies. 
//
#ifndef POS_POLICY_H
#define POS_POLICY_H

#include "utils.h"
#include "model.h"
#include "MarkovTree.h"
#include "tag.h"
#include "ThreadPool.h"
#include <boost/program_options.hpp>

#define POLICY_MARKOV_CHAIN_MAXDEPTH 10000

class Policy {
public:
  Policy(ModelPtr model, const boost::program_options::variables_map& vm);
  ~Policy();

  // results in terms of test samples.
  class Result {
  public:
    Result(const Corpus& corpus);
    std::vector<MarkovTreeNodePtr> nodes;
    Corpus corpus;
    double score;
  };
  typedef std::shared_ptr<Result> ResultPtr;
  inline static ResultPtr makeResultPtr(const Corpus& corpus) {
    return ResultPtr(new Result(corpus));
  }

  // run test on corpus.
  // return: accuracy on the test set.
  ResultPtr test(const Corpus& testCorpus);
  void test(ResultPtr result);

  // apply gradient from on samples.
  virtual void gradient(MarkovTree& tree);

  // run training on corpus.
  virtual void train(const Corpus& corpus);

  // sample node, default uses Gibbs sampling.
  virtual void sampleTest(int tid, MarkovTreeNodePtr node);

  // sample node, for training. default: call sampleTest.
  virtual void sample(int tid, MarkovTreeNodePtr node);
  
  // return a number referring to the transition kernel to use.
  // return = -1 : stop the markov chain.
  // o.w. return a natural number representing a choice.
  virtual int policy(MarkovTreeNodePtr node) = 0;

  // estimate the reward of a MarkovTree node (default: -dist).
  virtual double reward(MarkovTreeNodePtr node);

  // extract features from node.
  virtual FeaturePointer extractFeatures(MarkovTreeNodePtr node, int pos);

  // log information about node.
  virtual void logNode(MarkovTreeNodePtr node);

  // reset log.
  void resetLog(std::shared_ptr<XMLlog> new_lg);
protected:
  // const environment. 
  ParamPointer wordent, wordfreq;
  double wordent_mean, wordfreq_mean;
  Vector2d tag_bigram;
  std::vector<double> tag_unigram_start;
  const std::string name;
  const size_t K;
  const size_t test_count, train_count;
  const double eta;
  const bool verbose;

  // global environment.
  ModelPtr model;
  objcokus rng;
  std::shared_ptr<XMLlog> lg;
  ParamPointer param, G2;

  // parallel environment.
  ThreadPool<MarkovTreeNodePtr> thread_pool, test_thread_pool;    
};


// baseline policy of selecting Gibbs sampling kernels 
// just based on Gibbs sweeping.
class GibbsPolicy : public Policy {
public:
  GibbsPolicy(ModelPtr model, const boost::program_options::variables_map& vm);

  // policy: first make an entire pass over the sequence. 
  //	     second/third pass only update words with entropy exceeding threshold.
  int policy(MarkovTreeNodePtr node);

  size_t T; // how many sweeps.
};

// baseline policy of selecting Gibbs sampling kernels 
// just based on thresholding entropy of tags for words.
class EntropyPolicy : public Policy   {
public:
  EntropyPolicy(ModelPtr model, const boost::program_options::variables_map& vm);

  // policy: first make an entire pass over the sequence. 
  //	     second/third pass only update words with entropy exceeding threshold.
  int policy(MarkovTreeNodePtr node);

private:
  double threshold;   // entropy threshold = log(threshold).
};

// cyclic policy, 
// first sweep samples everything. 
// subsequent sweeps use logistic regression to predict whether to sample. 
// at end of every sweep, predict stop or not.
class CyclicPolicy : public Policy {
public:
  CyclicPolicy(ModelPtr model, const boost::program_options::variables_map& vm);

  int policy(MarkovTreeNodePtr node); 
  
  // reward = -dist - c * (depth+1).
  double reward(MarkovTreeNodePtr node);
  
  // extract features from node.
  virtual FeaturePointer extractFeatures(MarkovTreeNodePtr node, int pos);

protected:
  double c;          // regularization of computation.
};

// policy based on learning value function.
// virtual class that overwrites the training function.
class CyclicValuePolicy : public CyclicPolicy {
public:
  CyclicValuePolicy(ModelPtr model, const boost::program_options::variables_map& vm);

  int policy(MarkovTreeNodePtr node);

  // sample for training.
  void sample(int tid, MarkovTreeNodePtr node);

  // training.
  void train(const Corpus& corpus);

  // overload gradient, add exponentiated gradient for *c*.
  virtual void gradient(MarkovTree& tree);

private:
  static const bool lets_resp_reward = false;
  std::vector<std::pair<double, double> > resp_reward; // resp, reward pair.   
};
#endif
