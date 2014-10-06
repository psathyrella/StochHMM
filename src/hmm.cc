#include "hmm.h"

namespace ham {
// ----------------------------------------------------------------------------------------
model::model() : overall_gene_prob_(0),finalized(false),initial_(NULL) {
  ending_ = new State;
}

// ----------------------------------------------------------------------------------------
void model::parse(string infname) {
  YAML::Node config = YAML::LoadFile(infname);
  name_ = config["name"].as<string>();
  overall_gene_prob_ = config["extras"]["gene_prob"].as<double>();
  
  YAML::Node tracks(config["tracks"]);
  for (YAML::const_iterator it=tracks.begin(); it!=tracks.end(); ++it) {
    Track *trk = new Track;
    trk->setName(it->first.as<string>());
    for (size_t ic=0; ic<it->second.size(); ++ic)
      trk->addAlphabetChar(it->second[ic].as<string>());
    tracks_.push_back(trk);
  }

  vector<string> state_names;
  for (size_t is=0; is<config["states"].size(); ++is) {
    string name = config["states"][is]["name"].as<string>();
    for (auto chkname: state_names) {
      if (name == chkname) {
	cerr << "ERROR added two states with name \"" << name << "\"" << endl;
	assert(0);
      }
    }
    state_names.push_back(name);
  }

  for (size_t ist=0; ist<state_names.size(); ++ist) {
    State *st(new State);
    st->parse(config["states"][ist], state_names, tracks_);
    // st->print();

    if (st->name() == "init") {
      initial_ = st;
      stateByName[st->name()] = st;  // TODO is this (stateByName) actually used?
    } else {
      assert(states_.size() < STATE_MAX);
      states_.push_back(st);
      stateByName[st->name()] = st;
    }
  }
      
  //Post process states to create final state with only transitions from filled out
  finalize();
}
      
// ----------------------------------------------------------------------------------------
void model::add_state(State* st){
  assert(states_.size() < STATE_MAX);
  states_.push_back(st);
  stateByName[st->name()]=st;
  return;
};
  
//!Get pointer to state by state name
//!\param txt String name of state
//!\return pointer to state if it exists;
//!\return NULL if state doesn't exist in model
State* model::state(const string& txt){
  if (stateByName.count(txt)){
    return stateByName[txt];
  }
  else {
    return NULL;
  }
}
      
  
// ----------------------------------------------------------------------------------------
//!Finalize the model before performing decoding
//!Sets transitions, checks labels, Determines if model is basic or requires intermittent tracebacks
void model::finalize() {
  if (!finalized) {
    //Add States To Transitions
    set<string> labels;
    set<string> name;
                      
    //Create temporary hash of states for layout
    for(size_t i=0;i<states_.size();i++){
      labels.insert(states_[i]->label());
      // gff.insert(states_[i]->getGFF());
      // gff.insert(states_[i]->getName());
    }
                      
    for (size_t i=0; i < states_.size() ; ++i){
      states_[i]->setIter(i);
    }
          
    //Add states To and From transition
    for(size_t i=0;i<states_.size();i++){
      _addStateToFromTransition(states_[i]);
    }
                      
    _addStateToFromTransition(initial_);
                      
    //Now that we've seen all the states in the model
    //We need to fix the States transitions vector transi, so that the state
    //iterator correlates to the position within the vector
    for(size_t i=0;i<states_.size();i++){
      states_[i]->_finalizeTransitions(stateByName);
    }
    initial_->_finalizeTransitions(stateByName);
          
    //Check to see if model is basic model
    //Meaning that the model doesn't call outside functions or perform
    //Tracebacks for explicit duration.
    //If explicit duration exist then we'll keep track of which states
    //they are in explicit_duration_states
    //            for(size_t i=0;i<states.size();i++){
    //                vector<transition*>* transitions = states[i]->getTransitions();
    //                for(size_t trans=0;trans<transitions->size();trans++){
    //                                      if ((*transitions)[trans] == NULL){
    //                                              continue;
    //                                      }
    //                                      
    //                    if ((*transitions)[trans]->FunctionDefined()){
    //                        basicModel=false;
    //                        break;
    //                    }
    //                    else if ((*transitions)[trans]->getTransitionType()!=STANDARD || (*transitions)[trans]->getTransitionType()!=LEXICAL){
    //                                              
    //                                              if ((*transitions)[trans]->getTransitionType() == DURATION){
    //                                                      (*explicit_duration_states)[i]=true;
    //                                              }
    //                                              
    //                        basicModel=false;
    //                        break;
    //                    }
    //                }
    //            }
                      
    checkTopology();
                      
                      
    // //Assign StateInfo
    // for(size_t i=0; i < states.size();i++){
    //   string& st_name = states[i]->getName();
    //   info.stateIterByName[st_name]=i;
    //   info.stateIterByLabel[st_name].push_back(i);
    //   info.stateIterByGff[st_name].push_back(i);
    // }
    finalized = true;
  }
  return;
}
      

// ----------------------------------------------------------------------------------------
void model::_addStateToFromTransition(State* st){
  vector<transition*>* trans;
      
  //Process Initial State
  trans = st->getTransitions();
  for(size_t i=0;i<trans->size();i++){
    State* temp;
    temp=this->state((*trans)[i]->getName());
    if (temp){
      st->addToState(temp); //Also add the ptr to state vector::to
      (*trans)[i]->setState(temp);
      if (st!=initial_){
        temp->addFromState(st);
      }
    }
  }
      
  if (st->endi){
    ending_->addFromState(st);
  }
}
  
// ----------------------------------------------------------------------------------------
bool model::checkTopology(){
  //!Check model topology
  //!Iterates through all states to check to see if there are any:
  //! 1. Orphaned States
  //! 2. Dead end States
  //! 3. Uncompleted States
              
  vector<bool> states_visited (states_.size(),false);
  vector<uint16_t> visited;
              
  bool ending_defined(false);
              
  _checkTopology(initial_, visited);
              
  while (visited.size()>0){
    uint16_t st_iter = visited.back();
    visited.pop_back();
                      
    if (!states_visited[st_iter]){
      vector<uint16_t> tmp_visited;
      _checkTopology(states_[st_iter],tmp_visited);
      size_t num_visited = tmp_visited.size();
                              
      //Check orphaned
      if (num_visited == 0 ){
        //No transitions
        //cerr << "Warning: State: "  << states_[st_iter]->getName() << " has no transitions defined\n";
      }
      else if (num_visited == 1 && tmp_visited[0] == st_iter){
        //Orphaned
        if(states_[st_iter]->getEnding() == NULL){
          cerr << "State: "  << states_[st_iter]->name() << " is an orphaned state that has only transition to itself\n";
        }
        //                                      else{
        //                                              cerr << "State: "  << states_[st_iter]->getName() << " may be an orphaned state that only has transitions to itself and END state.\n";
        //                                      }
      }
                              
      for(size_t i=0; i < tmp_visited.size(); i++){
        if (!states_visited[tmp_visited[i]]){
          visited.push_back(tmp_visited[i]);
        }
      }
                              
      states_visited[st_iter] = true;
    }
  }
              
  //Check for defined ending
  for(size_t i=0; i< states_.size() ; i++){
    if ( states_[i]->getEnding() != NULL){
      ending_defined = true;
      break;
    }
  }
              
  if (!ending_defined){
    cerr << "No END state defined in the model\n";
  }
              
  for(size_t i=0; i< states_visited.size(); i++){
    if (!states_visited[i]){
      cerr << "State: "  << states_[i]->name() << " doesn't have valid model topology\n\
                              Please check the model transitions\n";
      return false;
    }
  }
  return true;
}
      
// ----------------------------------------------------------------------------------------
void model::_checkTopology(State* st, vector<uint16_t>& visited){
  //Follow transitions to see if every state is visited
  for(size_t i = 0 ; i < st->transi->size() ; i++){
    if (st->transi->at(i) != NULL){
      visited.push_back(st->transi->at(i)->getState()->getIterator());
    }
                      
  }
  return;
}
}
