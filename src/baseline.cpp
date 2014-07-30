/* implementation of baseline sequence tagging models, including 
 * > independent logistic regression.
 * > CRF with Gibbs sampling. 
 */
#include "model.h"

using namespace std;
using namespace std::placeholders;

//////// Model CRF Gibbs ///////////////////////////////
ModelCRFGibbs::ModelCRFGibbs(const Corpus& corpus, int T, int B, int Q, double eta)
:Model(corpus, T, B, Q, eta) {
}

TagVector ModelCRFGibbs::sample(const Sentence& seq) { 
  TagVector vec;
  gradient(seq, &vec, false); 
  return vec;
}

FeaturePointer ModelCRFGibbs::extractFeatures(const Tag& tag) {
  FeaturePointer features = makeFeaturePointer();
  const vector<Token>& sen = tag.seq->seq;
  int seqlen = sen.size();
  // extract word features. 
  for(int si = 0; si < seqlen; si++) {
    stringstream ss;
    ss << sen[si].word << "-" << tag.tag[si];
    (*features)[ss.str()] = 1;
  }
  // extract bigram features.
  for(int si = 1; si < seqlen; si++) {
    stringstream ss;
    ss << "p-" << tag.tag[si-1] << "-" << tag.tag[si];
    (*features)[ss.str()] = 1;
  }
  return features;
}

ParamPointer ModelCRFGibbs::gradient(const Sentence& seq) {
  return this->gradient(seq, nullptr, true);
}

ParamPointer ModelCRFGibbs::gradient(const Sentence& seq, TagVector* samples, bool update_grad) {
  Tag tag(&seq, corpus, &rngs[0], param);
  Tag truth(seq, corpus, &rngs[0], param);
  FeaturePointer feat = this->extractFeatures(truth);
  ParamPointer gradient(new map<string, double>()); 
  for(int t = 0; t < T; t++) {
    for(int i = 0; i < seq.tag.size(); i++) 
      tag.proposeGibbs(i, bind(&ModelCRFGibbs::extractFeatures, this, _1));
    if(t < B) continue;
    if(update_grad)
      mapUpdate<double, double>(*gradient, *this->extractFeatures(tag));
  }
  if(samples)
    samples->push_back(shared_ptr<Tag>(new Tag(tag)));
  xmllog.begin("truth"); xmllog << seq.str() << endl; xmllog.end();
  xmllog.begin("tag"); xmllog << tag.str() << endl; xmllog.end();
  if(update_grad) {
    mapDivide<double>(*gradient, -(double)(T-B));
    mapUpdate<double, double>(*gradient, *feat);
  }
  return gradient;
}

////////// Simple Model (Independent Logit) ////////////
ModelSimple::ModelSimple(const Corpus& corpus, int T, int B, int Q, double eta)
:Model(corpus, T, B, Q, eta) {
}

TagVector ModelSimple::sample(const Sentence& seq) {
  TagVector vec;
  gradient(seq, &vec, false);
  return vec;
}

FeaturePointer ModelSimple::extractFeatures(const Tag& tag, int pos) {
  FeaturePointer features = makeFeaturePointer();
  const vector<Token>& sen = tag.seq->seq;
  // extract word features only.
  stringstream ss;
  ss << "simple-" << sen[pos].word << "-" << tag.tag[pos];
  (*features)[ss.str()] = 1;
  return features;
}

ParamPointer ModelSimple::gradient(const Sentence& seq) {
  return this->gradient(seq, nullptr, true);
}

ParamPointer ModelSimple::gradient(const Sentence& seq, TagVector* samples, bool update_grad) {
  Tag tag(&seq, corpus, &rngs[0], param);
  ParamPointer gradient(new map<string, double>());
  for(size_t i = 0; i < tag.size(); i++) {
    auto featExtract = [&] (const Tag& tag) -> FeaturePointer {
			  return this->extractFeatures(tag, i); 
			};
    ParamPointer g = tag.proposeGibbs(i, featExtract, true, false);
    if(update_grad) {
      mapUpdate<double, double>(*gradient, *g);
      mapUpdate<double, double>(*gradient, *featExtract(tag));
    }
  }
  if(samples)
    samples->push_back(shared_ptr<Tag>(new Tag(tag)));
  xmllog.begin("truth"); xmllog << seq.str() << endl; xmllog.end();
  xmllog.begin("tag"); xmllog << tag.str() << endl; xmllog.end();
  return gradient;
}

void ModelSimple::run(const Corpus& testCorpus, bool lets_test) {
  Corpus retagged(testCorpus);
  retagged.retag(this->corpus); // use training taggs. 
  xmllog.begin("train_simple");
  int numObservation = 0;
  for(int q = 0; q < Q0; q++) {
    for(const Sentence& seq : corpus.seqs) {
      xmllog.begin("example_"+to_string(numObservation));
      ParamPointer gradient = this->gradient(seq, nullptr, true);
      this->configStepsize(gradient, this->eta);
      this->adagrad(gradient);
      xmllog.end();
      numObservation++;
    }
  }
  xmllog.end();
  if(lets_test) {
    xmllog.begin("test");
    test(retagged);
    xmllog.end();
  }
}

////////// Incremental Gibbs Sampling /////////////////////////
ModelIncrGibbs::ModelIncrGibbs(const Corpus& corpus, int T, int B, int Q, double eta)
:ModelCRFGibbs(corpus, T, B, Q, eta) {
}

TagVector ModelIncrGibbs::sample(const Sentence& seq) {
  TagVector samples;
  gradient(seq, &samples, false);
  xmllog.begin("truth"); xmllog << seq.str() << endl; xmllog.end();
  xmllog.begin("tag"); xmllog << samples.back()->str() << endl; xmllog.end();
  return samples;
}

ParamPointer ModelIncrGibbs::gradient(const Sentence& seq) {
  return this->gradient(seq, nullptr, false);
}

ParamPointer ModelIncrGibbs::gradient(const Sentence& seq, TagVector* samples, bool update_grad) {
  Tag tag(&seq, corpus, &rngs[0], param);
  Tag mytag(tag);
  Tag truth(seq, corpus, &rngs[0], param);
  FeaturePointer feat = this->extractFeatures(truth);
  ParamPointer gradient(new map<string, double>());
  for(int i = 0; i < seq.tag.size(); i++) {
    ParamPointer g = tag.proposeGibbs(i, 
			  bind(&ModelCRFGibbs::extractFeatures, this, _1), true, false);
    mapUpdate<double, double>(*gradient, *g);
    mytag.tag[i] = tag.tag[i];
    tag.tag[i] = seq.tag[i];
    mapUpdate<double, double>(*gradient, *this->extractFeatures(tag)); 
  }
  if(samples)
    samples->push_back(shared_ptr<Tag>(new Tag(tag)));
  return gradient;
}