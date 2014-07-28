#ifndef POS_TAG_H
#define POS_TAG_H

#include <string>
#include "corpus.h"
#include "objcokus.h"

#include <map>
#include <memory>
#include <boost/random/uniform_int.hpp>

typedef std::pair<std::string, double> ParamItem;
typedef ParamItem FeatureItem;
typedef std::shared_ptr<std::map<std::string, double> > ParamPointer;
typedef ParamPointer FeaturePointer;
typedef std::vector<std::vector<double> > Vector2d;

inline static ParamPointer makeParamPointer() {
  return ParamPointer(new std::map<std::string, double>());
}
inline static FeaturePointer makeFeaturePointer() {
  return makeParamPointer();
}
inline static Vector2d makeVector2d(size_t m, size_t n, double c = 0.0) {
  Vector2d vec(m);
  for(size_t mi = 0; mi < m; mi++) vec[mi].resize(n, c);
  return vec;
}

inline static void copyParamFeatures(ParamPointer param, std::string prefix_from,
	      std::string prefix_to) {
  for(const std::pair<std::string, double>& pair : *param) {
    std::string key = pair.first;
    size_t pos = key.find(prefix_from);
    if(pos == std::string::npos) 
      continue;
    (*param)[prefix_to + key.substr(pos + prefix_from.length())] = pair.second;
  }
}

inline static std::string str(FeaturePointer features);

struct Tag {
public:
  const Sentence* seq;
  const Corpus& corpus;
  
  objcokus* rng;

  std::vector<int> tag;

  FeaturePointer features; 
  ParamPointer param;

  /* corpus should be training corpus, as its tag mapping would be used.
   * DO NOT use the test corpus, as it would confuse the tagging.
   */
  Tag(const Sentence* seq, const Corpus& corpus, 
     objcokus* rng, ParamPointer param);
  inline size_t size() const {return this->tag.size(); }
  void randomInit();
  ParamPointer proposeSimple(int pos, bool withgrad = false);
  ParamPointer proposeGibbs(int pos, bool withgrad = false);
  FeaturePointer extractSimpleFeatures(const std::vector<int>& tag, int pos);
  FeaturePointer extractFeatures(const std::vector<int>& tag);
  double score(FeaturePointer features); // return un-normalized log-score.
  std::string str(); 
};

#endif
