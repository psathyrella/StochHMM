#ifndef HAM_GLOMERATOR_H
#define HAM_GLOMERATOR_H

#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <functional>
#include <pthread.h>

#include "args.h"
#include "dphandler.h"
#include "clusterpath.h"
#include "text.h"

using namespace std;
namespace ham {

typedef pair<vector<string>, vector<string> > ClusterPair;

// ----------------------------------------------------------------------------------------
class Query {
public:
  string name_;
  vector<Sequence> seqs_;
  KBounds kbounds_;
  vector<string> only_genes_;
  double mean_mute_freq_;
  pair<string, string> parents_;  // queries that were joined to make this
};

// ----------------------------------------------------------------------------------------
class Glomerator {
public:
  Glomerator(HMMHolder &hmms, GermLines &gl, vector<vector<Sequence> > &qry_seq_list, Args *args, Track *track);
  ~Glomerator();
  void Cluster();
  double LogProbOfPartition(Partition &clusters, bool debug=false);
  void Merge(ClusterPath *path);

  void CacheNaiveSeqs();
  // NOTE don't remove these (yet, at least)
  // ClusterPair GetClustersToMergeForNaiveSeqGlomerate(set<vector<string> > &clusters, int max_per_cluster, bool merge_whatever_you_got);
  // void PrintClusterSizes(set<vector<string> > &clusters);
  // ClusterPair GetSmallBigClusters(set<vector<string> > &clusters);
  // void NaiveSeqGlomerate(int n_clusters);
  
  // Return the next (i.e. the <i_initial_partition_>th) initial partition in the list of initial partitions, and increment <i_initial_partition_>.
  // Also sets arguments <initial_path_index> and <logweight> to correspond to the returned partition.
  Partition GetAnInitialPartition(int &initial_path_index, double &logweight);

  void WritePartitions(vector<ClusterPath> &paths);
  void WriteAnnotations(vector<ClusterPath> &paths);
private:
  void ReadCacheFile();
  void WriteCacheLine(ofstream &ofs, string query);
  void WriteCacheFile();

  void PrintPartition(Partition &clusters, string extrastr);
  string CacheSizeString();
  string FinalString();
  void WriteStatus();  // write some progress info to file

  string ParentalString(pair<string, string> *parents);
  int CountMembers(string namestr);
  string ClusterSizeString(Partition *partition);
  string JoinNames(string name1, string name2, string delimiter=":");
  string JoinNameStrings(vector<Sequence> &strlist, string delimiter=":");
  string JoinSeqStrings(vector<Sequence> &strlist, string delimiter=":");
  string PrintStr(string queries);
  bool SeedMissing(string queries, string delimiter=":");

  double CalculateHfrac(string &seq_a, string &seq_b);
  double NaiveHfrac(string key_a, string key_b);

  string ChooseSubsetOfNames(string queries, int n_max);
  string GetNaiveSeqNameToCalculate(string actual_queries);  // convert between the actual queries/key we're interested in and the one we're going to calculate
  string GetLogProbNameToCalculate(string queries, int n_max);
  pair<string, string> GetLogProbPairOfNamesToCalculate(string actual_queries, pair<string, string> actual_parents);  // convert between the actual queries/key we're interested in and the one we're going to calculate
  bool FirstParentMuchBigger(string queries, string queries_other, int nmax);
  string FindNaiveSeqNameReplace(pair<string, string> *parents);
  string &GetNaiveSeq(string key, pair<string, string> *parents=nullptr);
  // double NormFactor(string name);
  double GetLogProb(string queries);
  double GetLogProbRatio(string key_a, string key_b);
  string CalculateNaiveSeq(string key, RecoEvent *event=nullptr);
  double CalculateLogProb(string queries);

  bool SameLength(vector<Sequence> &seqs, bool debug=false);
  void AddFailedQuery(string queries, string error_str);
  vector<Sequence> MergeSeqVectors(string name_a, string name_b);
  void UpdateLogProbTranslationsForAsymetrics(Query &qmerge);
  Query GetMergedQuery(string name_a, string name_b);

  bool LikelihoodRatioTooSmall(double lratio, int candidate_cluster_size);
  Partition GetSeededClusters(Partition &partition);
  pair<double, Query> FindHfracMerge(ClusterPath *path);
  pair<double, Query> FindLRatioMerge(ClusterPath *path);
  pair<double, Query> *ChooseRandomMerge(vector<pair<double, Query> > &potential_merges);

  Track *track_;
  Args *args_;
  DPHandler vtb_dph_, fwd_dph_;
  ofstream ofs_;

  Partition initial_partition_;

  map<string, bool> seed_missing_;  // also cache the presence of the seed in each cluster

  map<string, string> naive_seq_name_translations_;
  map<string, pair<string, string> > logprob_name_translations_;
  map<string, string> logprob_asymetric_translations_;
  map<string, string> name_subsets_;

  map<string, vector<Sequence> > seq_info_;  // NOTE it would be more memory-efficient to just keep track of vectors of keys here, and have Glomerator keep all the actual info
  map<string, vector<string> > only_genes_;
  map<string, KBounds> kbinfo_;
  map<string, float> mute_freqs_;  // overall mute freq for single sequences, mean overall mute freq for n-sets of sequences
  map<string, size_t> cdr3_lengths_;  // assumes/enforces that it's the same for all members of a cluster

  // These all include cached info from previous runs
  map<string, double> log_probs_;  
  map<string, double> naive_hfracs_;  // NOTE since this uses the joint key, it assumes there's only *one* way to get to a given cluster (this is similar to, but not quite the same as, the situation for log probs and naive seqs)
  map<string, double> lratios_;
  map<string, string> naive_seqs_;
  map<string, string> errors_;

  set<string> failed_queries_;

  set<string> initial_log_probs_, initial_naive_hfracs_, initial_naive_seqs_;  // keep track of the ones we read from the initial cache file so we can write only the new ones to the output cache file

  int n_fwd_calculated_, n_vtb_calculated_, n_hfrac_calculated_, n_hfrac_merges_, n_lratio_merges_;

  double asym_factor_;

  Partition *current_partition_;  // (a.t.m. only used for writing to status file)
  time_t last_status_write_time_;  // last time that we wrote our progress to a file
  FILE *progress_file_;
};

}
#endif
