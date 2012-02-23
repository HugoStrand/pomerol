#include "Lattice.h"
#include "LatticePresets.h"
#include <fstream>
#include <algorithm>

namespace Pomerol{

//
// Lattice::Site
//

Lattice::Site::Site()
{
};

Lattice::Site::Site(const std::string& Label, unsigned short OrbitalSize, unsigned short SpinSize):Label(Label), OrbitalSize(OrbitalSize), SpinSize(SpinSize)
{
};


std::ostream& operator<<(std::ostream& output, const Lattice::Site& out)
{
    output << "Site \"" << out.Label << "\", " << out.OrbitalSize << " orbital" << ((out.OrbitalSize>1)?"s":"") << ", " << out.SpinSize << " spin" << ((out.SpinSize>1)?"s":"") << ".";
	return output;
}

//
// Lattice::Term
//

Lattice::Term::Term (unsigned int N):N(N)
{
    Order.resize(N);
    SiteLabels.resize(N);
    Spins.resize(N);
    Orbitals.resize(N);
    for (unsigned int i=0; i<N; ++i) { Order[i]=false; SiteLabels[i]=""; Spins[i]=0; Orbitals[i]=0.0; }; 
    Value=0.0; 
};


Lattice::Term::Term(unsigned int N, bool * Order_, RealType Value_, std::string * SiteLabels_, unsigned short * Orbitals_, unsigned short *Spins_):
N(N)
{
  Order.assign( Order_, Order_+N );
  SiteLabels.assign( SiteLabels_, SiteLabels_+N );
  Spins.assign( Spins_, Spins_+N );
  Orbitals.assign( Orbitals_, Orbitals_+N );
  Value=Value_;
}

Lattice::Term::Term (const Lattice::Term &in):N(in.N), Order(in.Order), SiteLabels(in.SiteLabels), Spins(in.Spins), Orbitals(in.Orbitals), Value(in.Value)
{
};
unsigned int Lattice::Term::getOrder() const { return N; };

std::ostream& operator<< (std::ostream& output, const Lattice::Term& out)
{   
    output << out.Value << "*"; 
    for (unsigned int i=0; i<out.N; ++i) output << ((out.Order[i])?"c^{+}":"c") << "_{" << out.SiteLabels[i] << "," << out.Orbitals[i] << "," << out.Spins[i] << "}" ; 
    return output; 
};

//
// Lattice::TermStorage
//

Lattice::TermStorage::TermStorage()
{
};

int Lattice::TermStorage::addTerm(const Lattice::Term *T)
{
    unsigned int N = T->getOrder();
    Terms[N].push_back(new Term(*T));
    return 0;
};

const Lattice::TermList &Lattice::TermStorage::getTermList (unsigned int N)
{
    if (Terms.find(N)!=Terms.end()) 
        { 
            return Terms[N];
        }
    else return *(new TermList ());
};

//
// Lattice
//

Lattice::Lattice():Terms(new TermStorage)
{
};

Lattice::~Lattice(){
delete Terms;
};

void Lattice::printTerms(unsigned int n){
TermList Temp = Terms->getTermList(n);
for (TermList::const_iterator it1=Temp.begin(); it1!=Temp.end(); ++it1) {
    INFO(**it1 );
    };
}

void Lattice::addSite(Lattice::Site* S)
{
    Sites[S->Label]= S ;
}

void Lattice::addSite(const std::string &Label, unsigned short orbitals, unsigned short spins)
{
    addSite(new Lattice::Site(Label, orbitals, spins));
}

void Lattice::addTerm(const Lattice::Term *T)
{
    Terms->addTerm(T);
}

const Lattice::Site& Lattice::getSite(const std::string& Label)
{
    if (Sites.find(Label)!=Sites.end()) throw (exWrongLabel()); 
    return *Sites[Label];
}

const char* Lattice::exWrongLabel::what() const throw(){
    return "Wrong requested Label";
};

//
// JSONLattice
//

JSONLattice::JSONLattice(){
};


int JSONLattice::readin(const std::string &filename)
{
  Json::Value *root = new Json::Value;
  Json::Reader reader;
  std::ifstream in;
  in.open(filename.c_str());
  try
  {
    bool parsingSuccessful = reader.parse( in, *root );
    if ( !parsingSuccessful )
  	{
		std::cout  << "Failed to parse configuration\n";
		std::cout << reader.getFormatedErrorMessages();
        return 1;
  	}
  }
  catch (std::exception ErrorException)
  	{
		std::cout << ErrorException.what() << std::endl;
		exit(1);
  	}
  in.close();
  readSites((*root)["Sites"]);
  delete root;
  return 0;
};

void JSONLattice::readSites(Json::Value &JSONSites)
{
    Log.setDebugging(true);
    JSONLattice::JSONPresets Helper;
    for (Json::Value::iterator it=JSONSites.begin(); it!=JSONSites.end(); ++it){

        std::string Label = it.key().asString();
        bool preset = (*it)["Type"]!=Json::nullValue;

        if (preset) { 
            std::string preset_name=(*it)["Type"].asString();
            DEBUG(preset_name);
            if (Helper.SiteActions.find(preset_name)!=Helper.SiteActions.end()) (Helper.*Helper.SiteActions[preset_name])(this, Label, *it);
            else { 
                ERROR("No JSON preset " << preset_name << " found. Treating site as a generic one. ");
                preset = false;
                };
            }; // end of : if (preset)

        if (!preset) {
            DEBUG(Label);
            unsigned short Orbitals = (*it)["Orbitals"].asInt();
            unsigned short Spins=2;
            if ((*it)["Spins"]!=Json::nullValue) Spins=(*it)["Spins"].asInt();
            Lattice::Site *S = new Lattice::Site(Label, Orbitals, Spins);
            this->Sites[Label]=S;
            if ((*it)["Level"]!=Json::nullValue) { 
                Lattice::Presets::addLevel(this, Label, (*it)["Level"].asDouble());
                };
            };
        }
};

} // end of namespace Pomerol
