#include "stop.h"

namespace po = boost::program_options;
using namespace std;

Stop::Stop(ModelPtr model, const po::variables_map& vm) 
:model(model), T(vm["T"].as<int>()), B(vm["B"].as<int>()), 
 K(vm["K"].as<int>()), name(vm["name"].as<string>()), 
	thread_pool(vm["numThreads"].as<int>(), 
		      [&] (int tid, MarkovTreeNodePtr node) {
			this->sample(tid, node);
		      }),
	test_thread_pool(vm["numThreads"].as<int>(), 
		      [&] (int tid, MarkovTreeNodePtr node) {
			this->sampleTest(tid, node);
		      }), 
  stop_data(makeStopDataset()), param(makeParamPointer()), 
  G2(makeParamPointer()), eta(vm["eta"].as<double>()),
  c(vm["c"].as<double>()) {
  system(("mkdir "+name).c_str());
  rng.seedMT(time(0));
  // init const environment.
  wordent = model->tagEntropySimple();
  wordfreq = model->wordFrequencies();
  auto tag_bigram_unigram = model->tagBigram();
  tag_bigram = tag_bigram_unigram.first;
  tag_unigram_start = tag_bigram_unigram.second;
}

StopDatasetPtr Stop::explore(const Sentence& seq) {    
  MarkovTree tree;
  tree.root->tag = makeTagPtr(&seq, model->corpus, &rng, model->param);
  thread_pool.addWork(tree.root);
  thread_pool.waitFinish();
  return tree.generateStopDataset(tree.root, 0);
}

void Stop::sample(int tid, MarkovTreeNodePtr node) {
  while(true) {
    node->tag->rng = &thread_pool.rngs[tid];
    model->sample(*node->tag, 1);
    node->stop_feat = extractStopFeatures(node);
    node->log_weight = this->score(node) - c * node->depth;
    node->log_prior_weight = -DBL_MAX;
    if(node->depth == T) {
      node->log_prior_weight = 1;
      return;
    }else if(node->depth <= B) { // split node, aggregate data.
      node->compute_stop = true;
      for(size_t k = 0; k < K; k++) {
	node->children.push_back(makeMarkovTreeNode(node, *node->tag));
	thread_pool.addWork(node->children.back()); 
      }
      return;
    }else{
      node->children.push_back(makeMarkovTreeNode(node, *node->tag));
      node = node->children.back();
    }
  }
}

void Stop::sampleTest(int tid, MarkovTreeNodePtr node) {
  node->depth = 0;
  while(true) { // maximum level of inference = T.
    node->tag->rng = &test_thread_pool.rngs[tid];
    model->sample(*node->tag, 1);
    node->stop_feat = extractStopFeatures(node);
    node->resp = logisticFunc(::score(param, node->stop_feat));
    if(node->depth == T || 
      test_thread_pool.rngs[tid].random01() < node->resp) { // stop.
      return;
    }else{
      node->children.push_back(makeMarkovTreeNode(node, *node->tag));
      node = node->children.back();
    }
  }
}

double Stop::score(shared_ptr<MarkovTreeNode> node) {
  Tag& tag = *node->tag;
  const Sentence* seq = tag.seq;
  double score = 0.0;
  for(int i = 0; i < tag.size(); i++) {
    score -= (tag.tag[i] != seq->tag[i]);
  }
  return score;
}

FeaturePointer Stop::extractStopFeatures(MarkovTreeNodePtr node) {
  FeaturePointer feat = makeFeaturePointer();
  Tag& tag = *node->tag;
  const Sentence& seq = *tag.seq;
  size_t seqlen = tag.size();
  size_t taglen = model->corpus->tags.size();
  // feat: bias.
  (*feat)["bias-stopornot"] = 1.0;
  // feat: word len.
  (*feat)["len-stopornot"] = (double)seqlen; 
  (*feat)["len-inv-stopornot"] = 1/(double)seqlen;
  // feat: entropy and frequency.
  double max_ent = -DBL_MAX, ave_ent = 0.0;
  double max_freq = -DBL_MAX, ave_freq = 0.0;
  for(size_t t = 0; t < seqlen; t++) {
    string word = seq.seq[t].word;
    double ent = 0, freq = 0;
    if(wordent->find(word) == wordent->end())
      ent = log(taglen); // no word, use maxent.
    else
      ent = (*wordent)[word];
    if(ent > max_ent) max_ent = ent;
    ave_ent += ent;

    if(wordfreq->find(word) == wordfreq->end())
      freq = log(model->corpus->total_words);
    else
      freq = (*wordfreq)[word];
    if(freq > max_freq) max_freq = freq;
    ave_freq += freq;
  }
  ave_ent /= seqlen;
  ave_freq /= seqlen;
  (*feat)["max-ent"] = max_ent;
  (*feat)["max-freq"] = max_freq;
  (*feat)["ave-ent"] = ave_ent;
  (*feat)["ave-freq"] = ave_freq;
  // feat: avg sample path length.
  int L = 3, l = 0;
  double dist = 0.0;
  shared_ptr<MarkovTreeNode> p = node;
  while(p->depth >= 1 && L >= 0) {
    auto pf = p->parent.lock();
    if(pf == nullptr) throw "MarkovTreeNode father is expired.";
    dist += p->tag->distance(*pf->tag);
    l++; L--;
    p = pf;
  }
  if(l > 0) dist /= l;
  (*feat)["len-sample-path"] = dist;
  // log probability of current sample in terms of marginal training stats.
  double logprob = tag_unigram_start[tag.tag[0]];
  for(size_t t = 1; t < seqlen; t++) 
    logprob += tag_bigram[tag.tag[t-1]][tag.tag[t]];
  (*feat)["log-prob-tag-bigram"] = logprob;
  return feat;
}

void Stop::run(const Corpus& corpus) {
  cout << "> run " << endl;
  // aggregate dataset.
  Corpus retagged(corpus);
  retagged.retag(*model->corpus); // use training taggs. 
  StopDatasetPtr stop_data = makeStopDataset();
  int count = 0;
  model->xmllog.begin("train");
  for(const Sentence& seq : corpus.seqs) { 
    cout << "count: " << count++ << endl;
    StopDatasetPtr dataset = this->explore(seq);
    mergeStopDataset(stop_data, this->explore(seq)); 
    // train logistic regression. 
    StopDatasetKeyContainer::iterator key_iter;
    StopDatasetValueContainer::iterator R_iter;
    StopDatasetValueContainer::iterator epR_iter;
    StopDatasetSeqContainer::iterator seq_iter;
    for(key_iter = std::get<0>(*dataset).begin(), R_iter = std::get<1>(*dataset).begin(),
	epR_iter = std::get<2>(*dataset).begin(), seq_iter = std::get<3>(*dataset).begin();
	key_iter != std::get<0>(*dataset).end() && R_iter != std::get<1>(*dataset).end() 
	&& epR_iter != std::get<2>(*dataset).end() && seq_iter != std::get<3>(*dataset).end(); 
	key_iter++, R_iter++, epR_iter++, seq_iter++) {
      model->xmllog << **key_iter << endl;
      double resp = logisticFunc(::score(param, *key_iter));
      double R_max = max(*R_iter, *epR_iter);
      double sc = log(resp * exp(*R_iter - R_max) + (1 - resp) * exp(*epR_iter - R_max)) + R_max;
      model->xmllog << "R: " << *R_iter << endl;
      model->xmllog << "epR: " << *epR_iter << endl;
      model->xmllog << "resp: " << ::score(param, *key_iter) << endl;
      model->xmllog << "score: " << sc << endl;
      ParamPointer gradient = makeParamPointer();
      mapUpdate<double, double>(*gradient, **key_iter, resp * (1 - resp) * (exp(*R_iter - R_max) - exp(*epR_iter - R_max)) / (exp(sc - R_max)));

      for(const pair<string, double>& p : *gradient) {
	mapUpdate(*G2, p.first, p.second * p.second);
	mapUpdate(*param, p.first, eta * p.second/sqrt(1e-4 + (*G2)[p.first]));
      }
    }
  }
  model->xmllog.end();
  stop_data_log = shared_ptr<XMLlog>(new XMLlog(name+"/stopdata.xml"));
  logStopDataset(stop_data, *this->stop_data_log);
  stop_data_log->end(); 
}


double Stop::test(const Corpus& testCorpus) {
  cout << "> test " << endl;
  Corpus retagged(testCorpus);
  retagged.retag(*model->corpus); // use training taggs. 
  vector<MarkovTreeNodePtr> result;
  int count = 0;
  for(const Sentence& seq : retagged.seqs) {
    MarkovTreeNodePtr node = makeMarkovTreeNode(nullptr);
    node->tag = makeTagPtr(&seq, model->corpus, &rng, model->param);
    test_thread_pool.addWork(node);
    result.push_back(node);
    count++;
  }
  test_thread_pool.waitFinish();
  int hit_count = 0, pred_count = 0;
  count = 0;
  XMLlog& lg = model->xmllog;
  lg.begin("param");
    lg << *param << endl;
  lg.end();
  for(MarkovTreeNodePtr node : result) {
    while(node->children.size() > 0) node = node->children[0];
    lg.begin("example_"+to_string(count));
      lg.begin("time"); lg << node->depth+1 << endl; lg.end();
      lg.begin("truth"); lg << node->tag->seq->str() << endl; lg.end();
      lg.begin("tag"); lg << node->tag->str() << endl; lg.end();
      lg.begin("resp"); lg << node->resp << endl; lg.end();
      lg.begin("feat"); lg << *node->stop_feat << endl; lg.end();
      int hits = 0;
      for(size_t i = 0; i < node->tag->size(); i++) {
	if(node->tag->tag[i] == node->tag->seq->tag[i]) {
	  hits++;
	}
      }
      lg.begin("dist"); lg << node->tag->size()-hits << endl; lg.end();
      hit_count += hits;
      pred_count += node->tag->size();
      count++;
    lg.end();
  }
  double acc = (double)hit_count/pred_count;
  lg.begin("accuracy");
    lg << acc << endl;
  lg.end();
  return acc;
}
