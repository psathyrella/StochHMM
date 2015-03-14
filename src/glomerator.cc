#include "glomerator.h"

namespace ham {

Glomerator::Glomerator(HMMHolder &hmms, GermLines &gl, vector<Sequences> &qry_seq_list, Args *args):  // reminder: <qry_seq_list> is a list of lists
  hmms_(hmms),
  gl_(gl),
  args_(args),
  max_log_prob_of_partition_(-INFINITY),
  finished_(false)
{
  // convert input vector to maps
  for(size_t iqry = 0; iqry < qry_seq_list.size(); iqry++) {
    string key(qry_seq_list[iqry].name_str(":"));

    KSet kmin(args->integers_["k_v_min"][iqry], args->integers_["k_d_min"][iqry]);
    KSet kmax(args->integers_["k_v_max"][iqry], args->integers_["k_d_max"][iqry]);

    KBounds kb(kmin, kmax);
    info_[key] = qry_seq_list[iqry];  // NOTE this is probably kind of inefficient to remake the Sequences all the time
    only_genes_[key] = args->str_lists_["only_genes"][iqry];
    kbinfo_[key] = kb;
  }

  ReadCachedLogProbs(args->incachefile());
  ofs_.open(args_->outfile());
  ofs_ << "partition,score,errors" << endl;
}

// ----------------------------------------------------------------------------------------
void Glomerator::Cluster() {
  vector<string> initial_partition(GetClusterList(info_));
  if(args_->debug())
    PrintPartition(initial_partition, "initial");

  do {
    Merge();
  } while(!finished_);

  if(args_->debug()) {
    cout << "-----------------" << endl;  
    PrintPartition(best_partition_, "best");
  }

  // TODO oh, damn, if the initial partition is better than any subsequent ones this breaks
  WritePartition(initial_partition, LogProbOfPartition(initial_partition));
  for(auto &pr : list_of_partitions_)
    WritePartition(pr.second, pr.first);

  // TODO really pass the cachefile inside args_? maybe do something cleaner
  WriteCachedLogProbs();

  ofs_.close();
}

// ----------------------------------------------------------------------------------------
void Glomerator::ReadCachedLogProbs(string fname) {
  if(fname == "")
    return;

  ifstream ifs(fname);
  assert(ifs.is_open());
  string line;

  // check the header is right TODO should write a general csv reader
  getline(ifs, line);
  vector<string> headstrs(SplitString(line, ","));
  cout << "x" << headstrs[0] << "x" << headstrs[1] << "x" << endl;
  assert(headstrs[0].find("unique_ids") == 0);
  assert(headstrs[1].find("score") == 0);

  while(getline(ifs, line)) {
    vector<string> column_list = SplitString(line, ",");
    assert(column_list.size() == 2);
    string unique_ids(column_list[0]);
    double logprob(stof(column_list[1]));
    log_probs_[unique_ids] = logprob;
  }
}

// ----------------------------------------------------------------------------------------
// return a vector consisting of the keys in <partinfo>
vector<string> Glomerator::GetClusterList(map<string, Sequences> &partinfo) {
  vector<string> clusters;
  for(auto &kv : partinfo)
    clusters.push_back(kv.first);
  return clusters;
}

// ----------------------------------------------------------------------------------------
double Glomerator::LogProbOfPartition(vector<string> &clusters) {
  // get log prob of entire partition given by the keys in <partinfo> using the individual log probs in <log_probs>
  double total_log_prob(0.0);
  for(auto &key : clusters) {
    if(log_probs_.count(key) == 0)
      throw runtime_error("ERROR couldn't find key " + key + " in cached log probs\n");
    total_log_prob = AddWithMinusInfinities(total_log_prob, log_probs_[key]);
  }
  return total_log_prob;
}

// ----------------------------------------------------------------------------------------
void Glomerator::PrintPartition(vector<string> &clusters, string extrastr) {
  const char *extra_cstr(extrastr.c_str());
  if(log_probs_.size() == 0)
    printf("    %s partition\n", extra_cstr);
  else
    printf("    %-8.2f %s partition\n", LogProbOfPartition(clusters), extra_cstr);
  for(auto &key : clusters)
    cout << "          " << key << endl;
}

// ----------------------------------------------------------------------------------------
void Glomerator::WriteCachedLogProbs() {
  ofstream log_prob_ofs(args_->outcachefile());
  assert(log_prob_ofs.is_open());
  log_prob_ofs << "unique_ids,score" << endl;
  for(auto &kv : log_probs_)
    log_prob_ofs << kv.first << "," << kv.second << endl;
  log_prob_ofs.close();
}

// ----------------------------------------------------------------------------------------
void Glomerator::WritePartition(vector<string> partition, double log_prob) {
  for(size_t ic=0; ic<partition.size(); ++ic) {
    if(ic > 0)
      ofs_ << ";";
    ofs_ << partition[ic];
  }
  ofs_ << ",";
  ofs_ << log_prob << ",";
  ofs_ << "n/a\n";
}

// ----------------------------------------------------------------------------------------
int Glomerator::MinimalHammingDistance(Sequences &seqs_a, Sequences &seqs_b) {
  // Minimal hamming distance between any sequence in <seqs_a> and any sequence in <seqs_b>
  // NOTE for now, we require sequences are the same length (otherwise we have to deal with alignming them which is what we would call a CAN OF WORMS.
  assert(seqs_a.n_seqs() > 0 && seqs_b.n_seqs() > 0);
  int min_distance(seqs_a[0].size());
  for(size_t is=0; is<seqs_a.n_seqs(); ++is) {
    for(size_t js=0; js<seqs_b.n_seqs(); ++js) {
      Sequence seq_a = seqs_a[is];
      Sequence seq_b = seqs_b[js];
      assert(seq_a.size() == seq_b.size());
      int distance(0);
      for(size_t ic=0; ic<seq_a.size(); ++ic) {
	if(seq_a[ic] != seq_b[ic])
	  ++distance;
      }
      if(distance < min_distance)
	min_distance = distance;
    }
  }
  return min_distance;
}

// ----------------------------------------------------------------------------------------
void Glomerator::GetResult(JobHolder &jh, string name, Sequences &seqs, KBounds &kbounds, string &errors) {
  if(log_probs_.count(name))  // already did it
    return;
    
  Result result(kbounds);
  bool stop(false);
  do {
    result = jh.Run(seqs, kbounds);
    kbounds = result.better_kbounds();
    stop = !result.boundary_error() || result.could_not_expand();  // stop if the max is not on the boundary, or if the boundary's at zero or the sequence length
    if(args_->debug() && !stop)
      cout << "      expand and run again" << endl;  // note that subsequent runs are much faster than the first one because of chunk caching
  } while(!stop);

  log_probs_[name] = result.total_score();
  if(result.boundary_error())
    errors = "boundary";
}

// ----------------------------------------------------------------------------------------
// perform one merge step, i.e. find the two "nearest" clusters and merge 'em
void Glomerator::Merge() {
  double max_log_prob(-INFINITY);
  pair<string, string> max_pair; // pair of clusters with largest log prob (i.e. the ratio of their prob together to their prob apart is largest)
  vector<string> max_only_genes;
  KBounds max_kbounds;

  set<string> already_done;  // keeps track of which  a-b pairs we've already done
  for(auto &kv_a : info_) {  // note that c++ maps are ordered
    for(auto &kv_b : info_) {
      if(kv_a.first == kv_b.first) continue;
      vector<string> names{kv_a.first, kv_b.first};
      sort(names.begin(), names.end());
      string bothnamestr(names[0] + ":" + names[1]);
      if(already_done.count(bothnamestr))  // already did this pair
	continue;
      else
	already_done.insert(bothnamestr);

      Sequences a_seqs(kv_a.second), b_seqs(kv_b.second);
      // TODO cache hamming fraction as well
      double hamming_fraction = float(MinimalHammingDistance(a_seqs, b_seqs)) / a_seqs[0].size();  // minimal_hamming_distance() will fail if the seqs aren't all the same length
      bool TMP_only_get_single_log_probs(false);  // TODO replace this var with something sensibler
      if(hamming_fraction > args_->hamming_fraction_cutoff()) {  // if all sequences in a are too far away from all sequences in b
	if(log_probs_.count(kv_a.first) == 0 || log_probs_.count(kv_b.first) == 0)
	  TMP_only_get_single_log_probs = true;
	else
	  continue;
      }

      // TODO skip all this stuff if we already have all three of 'em cached
      Sequences ab_seqs(a_seqs.Union(b_seqs));
      vector<string> ab_only_genes = only_genes_[kv_a.first];
      for(auto &g : only_genes_[kv_b.first])  // NOTE this will add duplicates (that's no big deal, though) OPTIMIZATION
	ab_only_genes.push_back(g);
      KBounds ab_kbounds = kbinfo_[kv_a.first].LogicalOr(kbinfo_[kv_b.first]);

      JobHolder jh(gl_, hmms_, args_->algorithm(), ab_only_genes);  // NOTE it's an ok approximation to compare log probs for sequence sets that were run with different kbounds, but (I'm pretty sure) we do need to run them with the same set of genes. EDIT hm, well, maybe not. Anywa, it's ok for now
      // TODO make sure that using <ab_only_genes> doesn't introduce some bias
      jh.SetDebug(args_->debug());
      jh.SetChunkCache(args_->chunk_cache());
      jh.SetNBestEvents(args_->n_best_events());

      string errors;
      // NOTE the error from using the single kbounds rather than the OR seems to be around a part in a thousand or less
      GetResult(jh, kv_a.first, a_seqs, kbinfo_[kv_a.first], errors);
      GetResult(jh, kv_b.first, b_seqs, kbinfo_[kv_b.first], errors);
      if(TMP_only_get_single_log_probs)
	continue;
      GetResult(jh, bothnamestr, ab_seqs, ab_kbounds, errors);

      double bayes_factor(log_probs_[bothnamestr] - log_probs_[kv_a.first] - log_probs_[kv_b.first]);  // REMINDER a, b not necessarily same order as names[0], names[1]
      if(args_->debug()) {
	printf("   %8.3f = ", bayes_factor);
	printf("%2s %8.2f", "", log_probs_[bothnamestr]);
	printf(" - %8.2f - %8.2f", log_probs_[kv_a.first], log_probs_[kv_b.first]);
	printf("\n");
      }

      if(bayes_factor > max_log_prob) {
	max_log_prob = bayes_factor;
	max_pair = pair<string, string>(kv_a.first, kv_b.first);  // REMINDER not always same order as names[0], names[1]
	max_only_genes = ab_only_genes;
	max_kbounds = ab_kbounds;
      }
    }
  }

  // if <info_> only has one cluster, if hamming is too large between all remaining clusters/remaining bayes factors are -INFINITY
  if(max_log_prob == -INFINITY) {
    finished_ = true;
    return;
  }

  // then merge the two best clusters
  vector<string> max_names{max_pair.first, max_pair.second};
  sort(max_names.begin(), max_names.end());
  Sequences max_seqs(info_[max_names[0]].Union(info_[max_names[1]]));  // NOTE this will give the ordering {<first seqs>, <second seqs>}, which should be the same as in <max_name_str>. But I don't think it'd hurt anything if the sequences and names were in a different order
  string max_name_str(max_names[0] + ":" + max_names[1]);  // NOTE the names[i] are not sorted *within* themselves, but <names> itself is sorted. This is ok, because we will never again encounter these sequences separately
  info_[max_name_str] = max_seqs;
  kbinfo_[max_name_str] = max_kbounds;
  only_genes_[max_name_str] = max_only_genes;

  info_.erase(max_pair.first);
  info_.erase(max_pair.second);
  kbinfo_.erase(max_pair.first);
  kbinfo_.erase(max_pair.second);
  only_genes_.erase(max_pair.first);
  only_genes_.erase(max_pair.second);

  if(args_->debug())
    printf("       merged %-8.2f %s and %s\n", max_log_prob, max_pair.first.c_str(), max_pair.second.c_str());

  vector<string> partition(GetClusterList(info_));
  if(args_->debug())
    PrintPartition(partition, "current");
  double total_log_prob = LogProbOfPartition(partition);

  list_of_partitions_.push_back(pair<double, vector<string> >(total_log_prob, partition));

  if(total_log_prob > max_log_prob_of_partition_) {
    best_partition_ = partition;
    max_log_prob_of_partition_ = total_log_prob;
  }

  if(max_log_prob_of_partition_ - total_log_prob > 1000.0) {  // stop if we've moved too far past the maximum
    cout << "    stopping after drop " << max_log_prob_of_partition_ << " --> " << total_log_prob << endl;
    finished_ = true;  // NOTE this will not play well with multiple maxima, but I'm pretty sure we shouldn't be getting those
  }
}

}