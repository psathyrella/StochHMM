#ifndef STOCHHMM_TEXT_H
#define STOCHHMM_TEXT_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdlib.h>

namespace stochhmm{
        
  /*! \defgroup Text  Text Handling
   *  
   *
   */
        
        
    
  /*! \class stringList
    \brief Stringlist is an list of strings that contains parsed comments and 
    can be used to split string or remove whitespace from a string
     
  */

  class stringList{
  public:
    stringList();
    stringList(std::string&,std::string&,std::string&,bool);
        
    //MUTATORS
    //! Set remove whitespace flag. When splitting string it will remove Whitespace first and then split the string.
    inline void setRemoveWS(){removeWS=true;}; //!<
        
        
    //! Unset remove whitespace flag. When splitting string it will remove Whitespace first and then split the string.
    inline void unsetRemoveWS(){removeWS=false;};
        
    //! Set whitespace character. When splitting string it will remove defined whitespace characters before splitting the string.
    //! \param txt String of whitespace characters to remove from sequence
    inline void setWhitespace(const std::string& txt){whitespace=txt;};
        
    //! Set delimiter characters.  String will be split on these characters
    //! \param txt String of text-delimiters to use in splitting string
    inline void setDelimiters(const std::string& txt){delimiters=txt;};
        
    void getLine(std::istream&);
    void processString(std::string&); //Remove comment, whitespace, and split string
    void splitString(const std::string&); //Split string with delimiter defined in stringList
    void splitString(const std::string&, const std::string&); //Split string with given delimiter
    void splitString(const std::string&, size_t);
    void splitND(const std::string&, const std::string&);
    void removeLWS(const std::string&);
    void removeComments();
        
    //! Clears all values from the stringList, including comments, whitespace, and delimiters
    inline void clear(){lines.clear();comment.clear();whitespace.clear();delimiters.clear();};
        
    bool fromTable(std::string&);
    bool fromTable(std::istream&);
    bool fromTrack(std::string&);
    bool fromTrack(std::istream&);
    bool fromAlpha(const std::string&,size_t);
    bool fromAlpha(std::istream&,size_t);
    bool fromTxt(std::string&);
    bool fromTxt(std::istream&);
    bool fromNext(std::string&);
    bool fromNext(std::istream&);
    bool fromDef(std::string&,std::string&, std::string&);
    bool fromDef(std::istream&,std::string&, std::string&);
        
    //! Add string to StringList
    //! \param txt String to add to the stringList
    inline void operator<< (std::string& txt){lines.push_back(txt);};
        
    //! Add string to StringList
    //! \param txt String to add to the stringList
    inline void push_back  (std::string& txt){lines.push_back(txt);};
        
    std::string pop_ith(size_t);  // erase the <pos>th 
        
    //! Copy StringList
    //! \param lst stringList to copy
    inline void operator= (stringList& lst){lines=lst.lines; comment=lst.comment; whitespace=lst.whitespace; delimiters=lst.delimiters;};
        
    //ACCESSORS
    //! Access string at iterator 
    //! \param iter Position to return;
    inline std::string& operator[](size_t iter){return lines[iter];};
        
    //! Return any comments parsed from the line
    inline std::string& getComment(){return comment;};
        
    //! Returns the list as a Vector of Doubles
    std::vector<double> toVecDouble();
        
    void toVecInt(std::vector<int>&);

        
    size_t indexOf(const std::string&);
    size_t indexOf(const std::string&,size_t);
    bool contains(const std::string&);
    bool containsExact(const std::string& txt);
    void print();
    std::string stringify();
        
    //! Return the amount of strings in the stringList 
    inline size_t size(){return lines.size();};
    
        
  private:
    void _splitIndividual(std::string&, size_t);
    bool foundAlphaDelimiter(const std::string&);
        
    std::vector<std::string> lines;
    std::string comment;
        
    bool removeWS;
    std::string whitespace;
    std::string delimiters;
  };


   
    
  stringList& splitString(const std::string&,const std::string&);
    
  void clear_whitespace(std::string&, std::string);
  void replaceChar(std::string&, char ,char);
  void removeLeadingWS(std::string&,const std::string&);
  std::string parseComment(std::string&,char);
  void getKeyValue(std::string&,std::string&,std::string&);

  std::string join(std::vector<int>&, char);
  std::string join(std::vector<size_t>&, char);
  std::string join(std::vector<short>&,char);
  std::string join(std::vector<double>&, char);
  std::string join(std::vector<std::string>&, char);

  std::string int_to_string(int);
  std::string int_to_string(size_t);
  std::string double_to_string(double);
  std::string double_to_string(float);
    
  bool stringToInt(std::string&, int&);
  bool stringToInt(std::string&, size_t&);
  bool stringToDouble(std::string&, double&);

  bool isNumeric(const std::string&);


  void split_line(std::vector<std::string>&, std::string& );

  stringList extractTag(std::string&);
  std::pair<size_t,size_t> balanced_brackets(const std::string&, const std::string& );
  std::pair<size_t,size_t> balanced_brackets(const std::string&, const std::string& , size_t );
    
  std::string slurpFile(std::string&);

  /*
    inline double convertToDouble (const std::string& s){
    istd::stringstream i(s);
    double x;
    if (!(i>>x)){
    throw BadConversion("convertToDouble(\"" + s "\")"));
    }
    return x;
    }
  */
        
  /**@}*/ //End of Text Handling Group
    
}
#endif /*TEXT_H*/