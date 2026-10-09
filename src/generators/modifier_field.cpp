#include "cropsim/generators/modifier_field.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <yaml-cpp/yaml.h>

namespace cropsim::generators {
namespace {
constexpr double pi = 3.141592653589793238462643383279502884;
constexpr double e = 2.718281828459045235360287471352662498;
constexpr std::uint64_t hash_basis = 1469598103934665603ULL;
constexpr std::uint64_t hash_prime = 1099511628211ULL;

std::uint64_t hash_name(std::string_view text) {
  auto result = hash_basis;
  for (const auto character : text) {
    result ^= static_cast<unsigned char>(character);
    result *= hash_prime;
  }
  return result;
}

bool contains(const Polygon2 &polygon, const Point2 point) {
  bool inside = false;
  for (std::size_t i = 0, j = polygon.vertices.size() - 1U;
       i < polygon.vertices.size(); j = i++) {
    const auto &a = polygon.vertices[i];
    const auto &b = polygon.vertices[j];
    const auto crossing = ((a.y > point.y) != (b.y > point.y)) &&
      point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x;
    inside = crossing ? !inside : inside;
  }
  return inside;
}

enum class Op { constant, x, y, u, v, field, add, sub, mul, div, power,
                neg, pos, function };
enum class Fn { sin, cos, tan, asin, acos, atan, atan2, exp, log, log10,
                sqrt, abs, floor, ceil, round, min, max, pow, clamp, smoothstep };
struct Instruction { Op op{}; double number{}; std::size_t index{}; Fn fn{}; int arity{}; };

class ExpressionParser final {
public:
  explicit ExpressionParser(std::string source) : source_(std::move(source)) {
    if (source_.size() > 4096U) throw std::invalid_argument("modifier expression exceeds 4096 characters");
  }
  std::vector<Instruction> parse() {
    expression(); skip();
    if (position_ != source_.size()) fail("unexpected token");
    if (code_.size() > 1024U) fail("expression exceeds 1024 instructions");
    return code_;
  }
  const std::vector<std::string> &references() const { return references_; }
private:
  void fail(const char *message) const { throw std::invalid_argument(std::string("modifier expression: ") + message); }
  void skip() { while (position_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[position_]))) ++position_; }
  bool take(char value) { skip(); if (position_ < source_.size() && source_[position_] == value) { ++position_; return true; } return false; }
  void enter() { if (++depth_ > 64U) fail("nesting exceeds 64"); }
  void leave() { --depth_; }
  void emit(Instruction instruction) { code_.push_back(instruction); if (code_.size() > 1024U) fail("expression exceeds 1024 instructions"); }
  void expression() { enter(); term(); while (true) { if (take('+')) { term(); emit({Op::add}); } else if (take('-')) { term(); emit({Op::sub}); } else break; } leave(); }
  void term() { unary(); while (true) { if (take('*')) { unary(); emit({Op::mul}); } else if (take('/')) { unary(); emit({Op::div}); } else break; } }
  void unary() { if (take('+')) { unary(); emit({Op::pos}); } else if (take('-')) { unary(); emit({Op::neg}); } else power(); }
  void power() { primary(); if (take('^')) { unary(); emit({Op::power}); } }
  std::string identifier() {
    skip(); const auto start = position_;
    if (position_ >= source_.size() || !(std::isalpha(static_cast<unsigned char>(source_[position_])) || source_[position_] == '_')) fail("expected identifier");
    while (position_ < source_.size() && (std::isalnum(static_cast<unsigned char>(source_[position_])) || source_[position_] == '_')) ++position_;
    return source_.substr(start, position_ - start);
  }
  void primary() {
    skip();
    if (take('(')) { expression(); if (!take(')')) fail("missing closing parenthesis"); return; }
    if (position_ < source_.size() && (std::isdigit(static_cast<unsigned char>(source_[position_])) || source_[position_] == '.')) {
      std::size_t used{}; const auto value = std::stod(source_.substr(position_), &used);
      if (!std::isfinite(value)) {
        fail("number must be finite");
      }
      position_ += used; emit({Op::constant, value}); return;
    }
    const auto name = identifier();
    if (take('(')) {
      int arity = 0; if (!take(')')) { do { expression(); ++arity; } while (take(',')); if (!take(')')) fail("missing function parenthesis"); }
      const std::array<std::pair<const char *, Fn>, 19> functions{{
        {"sin",Fn::sin},{"cos",Fn::cos},{"tan",Fn::tan},{"asin",Fn::asin},{"acos",Fn::acos},{"atan",Fn::atan},{"atan2",Fn::atan2},{"exp",Fn::exp},{"log",Fn::log},{"log10",Fn::log10},{"sqrt",Fn::sqrt},{"abs",Fn::abs},{"floor",Fn::floor},{"ceil",Fn::ceil},{"round",Fn::round},{"min",Fn::min},{"max",Fn::max},{"pow",Fn::pow},{"clamp",Fn::clamp}}};
      auto found = std::find_if(functions.begin(), functions.end(), [&](const auto &item){ return name == item.first; });
      Fn function{};
      if (found != functions.end()) function = found->second;
      else if (name == "smoothstep") function = Fn::smoothstep;
      else fail("unknown function");
      const auto expected = (function == Fn::atan2 || function == Fn::min || function == Fn::max || function == Fn::pow) ? 2 :
                            (function == Fn::clamp || function == Fn::smoothstep) ? 3 : 1;
      if (arity != expected) fail("wrong function argument count");
      emit({Op::function, 0.0, 0U, function, arity}); return;
    }
    if (name == "x") emit({Op::x}); else if (name == "y") emit({Op::y});
    else if (name == "u") emit({Op::u}); else if (name == "v") emit({Op::v});
    else if (name == "pi") emit({Op::constant, pi}); else if (name == "e") emit({Op::constant, e});
    else { auto it = std::find(references_.begin(), references_.end(), name); if (it == references_.end()) { references_.push_back(name); it = references_.end()-1; } emit({Op::field, 0.0, static_cast<std::size_t>(it-references_.begin())}); }
  }
  std::string source_; std::size_t position_{}; std::size_t depth_{};
  std::vector<Instruction> code_; std::vector<std::string> references_;
};

class ExpressionField final : public ModifierField {
public:
  explicit ExpressionField(const YAML::Node &node) {
    if (!node["expression"] || !node["expression"].IsScalar()) throw std::invalid_argument("expression field requires expression");
    ExpressionParser parser(node["expression"].as<std::string>()); code_ = parser.parse(); names_ = parser.references();
  }
  std::vector<std::string> dependencies() const override { return names_; }
  void bind(const std::unordered_map<std::string,std::size_t> &indices) override {
    for (auto &instruction : code_) if (instruction.op == Op::field) instruction.index = indices.at(names_.at(instruction.index));
  }
  double evaluate(double x, double y, double u, double v, const std::vector<double> &values) const override {
    std::array<double, 1025> stack{}; std::size_t size = 0;
    auto push=[&](double value){ stack[size++]=value; }; auto pop=[&](){ return stack[--size]; };
    for (const auto &in : code_) {
      switch (in.op) {
      case Op::constant: push(in.number); break; case Op::x: push(x); break; case Op::y: push(y); break; case Op::u: push(u); break; case Op::v: push(v); break; case Op::field: push(values[in.index]); break;
      case Op::add: { auto b=pop(),a=pop(); push(a+b); break; } case Op::sub: { auto b=pop(),a=pop(); push(a-b); break; } case Op::mul: { auto b=pop(),a=pop(); push(a*b); break; } case Op::div: { auto b=pop(),a=pop(); push(a/b); break; } case Op::power: { auto b=pop(),a=pop(); push(std::pow(a,b)); break; }
      case Op::neg: stack[size-1U] = -stack[size-1U]; break; case Op::pos: break;
      case Op::function: {
        const auto c = pop(); double result{};
        if (in.arity == 1) { switch(in.fn) { case Fn::sin:result=std::sin(c);break;case Fn::cos:result=std::cos(c);break;case Fn::tan:result=std::tan(c);break;case Fn::asin:result=std::asin(c);break;case Fn::acos:result=std::acos(c);break;case Fn::atan:result=std::atan(c);break;case Fn::exp:result=std::exp(c);break;case Fn::log:result=std::log(c);break;case Fn::log10:result=std::log10(c);break;case Fn::sqrt:result=std::sqrt(c);break;case Fn::abs:result=std::abs(c);break;case Fn::floor:result=std::floor(c);break;case Fn::ceil:result=std::ceil(c);break;case Fn::round:result=std::round(c);break;default:break;} }
        else { const auto b=pop(); if (in.arity==2) { const auto a=b; const auto d=c; switch(in.fn){case Fn::atan2:result=std::atan2(a,d);break;case Fn::min:result=std::min(a,d);break;case Fn::max:result=std::max(a,d);break;case Fn::pow:result=std::pow(a,d);break;default:break;} } else { const auto a=pop(); if(in.fn==Fn::clamp) result=std::clamp(a,b,c); else { const auto t=std::clamp((c-a)/(b-a),0.0,1.0); result=t*t*(3.0-2.0*t); } } }
        push(result); break;
      }}
    }
    if (size != 1U || !std::isfinite(stack[0])) throw std::runtime_error("modifier expression produced a non-finite result");
    return stack[0];
  }
private: std::vector<Instruction> code_; std::vector<std::string> names_;
};

std::uint64_t mix64(std::uint64_t value) { value += 0x9e3779b97f4a7c15ULL; value=(value^(value>>30U))*0xbf58476d1ce4e5b9ULL; value=(value^(value>>27U))*0x94d049bb133111ebULL; return value^(value>>31U); }
double gradient(std::int64_t ix, std::int64_t iy, std::uint64_t seed, double x, double y) {
  auto hash=mix64(seed ^ mix64(static_cast<std::uint64_t>(ix)) ^ (mix64(static_cast<std::uint64_t>(iy))<<1U));
  const auto angle=2.0*pi*(static_cast<double>(hash>>11U)+0.5)/9007199254740992.0;
  return std::cos(angle)*x+std::sin(angle)*y;
}
double noise(double x,double y,std::uint64_t seed) {
  const auto ix=static_cast<std::int64_t>(std::floor(x)), iy=static_cast<std::int64_t>(std::floor(y)); const auto fx=x-std::floor(x),fy=y-std::floor(y);
  const auto fade=[](double t){return t*t*t*(t*(t*6.0-15.0)+10.0);}; const auto lerp=[](double a,double b,double t){return a+(b-a)*t;};
  const auto a=lerp(gradient(ix,iy,seed,fx,fy),gradient(ix+1,iy,seed,fx-1,fy),fade(fx));
  const auto b=lerp(gradient(ix,iy+1,seed,fx,fy-1),gradient(ix+1,iy+1,seed,fx-1,fy-1),fade(fx)); return lerp(a,b,fade(fy))*1.4142135623730951;
}
class NoiseField final : public ModifierField {
public:
  NoiseField(const YAML::Node &n,std::uint64_t seed): wavelength_(n["wavelength"].as<double>()),octaves_(n["octaves"]?n["octaves"].as<std::size_t>():4U),lacunarity_(n["lacunarity"]?n["lacunarity"].as<double>():2.0),persistence_(n["persistence"]?n["persistence"].as<double>():0.5),amplitude_(n["amplitude"]?n["amplitude"].as<double>():1.0),offset_(n["offset"]?n["offset"].as<double>():0.0),seed_(seed) {
    if (!(wavelength_>0) || octaves_==0 || !(lacunarity_>0) || persistence_<0 || !std::isfinite(wavelength_+lacunarity_+persistence_+amplitude_+offset_)) throw std::invalid_argument("invalid fractal_noise parameters");
  }
  double evaluate(double x,double y,double,double,const std::vector<double>&) const override { double sum=0,weight=1,total=0,frequency=1.0/wavelength_; for(std::size_t i=0;i<octaves_;++i){sum+=noise(x*frequency,y*frequency,mix64(seed_+i))*weight;total+=weight;frequency*=lacunarity_;weight*=persistence_;} return offset_+amplitude_*std::clamp(sum/total,-1.0,1.0); }
private: double wavelength_;std::size_t octaves_;double lacunarity_,persistence_,amplitude_,offset_;std::uint64_t seed_;
};

struct Hotspot { double x,y,sx,sy,rotation,amplitude; };
double scalar(const YAML::Node &n,const char *name,double fallback,RandomStream *random=nullptr) { const auto v=n[name]; if(!v)return fallback;if(v.IsScalar())return v.as<double>();if(!v.IsMap())throw std::invalid_argument(std::string(name)+" must be scalar or distribution");const auto mean=v["mean"].as<double>();const auto lo=v["min"]?v["min"].as<double>():mean;const auto hi=v["max"]?v["max"].as<double>():mean;return random?lo+(hi-lo)*random->uniform_open():mean; }
class HotspotField final : public ModifierField {
public:
  HotspotField(const YAML::Node &n,const ModifierDomain &domain,std::uint64_t seed) {
    normalized_=n["coordinates"]&&n["coordinates"].as<std::string>()=="normalized"; const auto explicit_nodes=n["hotspots"];
    if(explicit_nodes&&n["count"])throw std::invalid_argument("hotspot explicit and generated modes are mutually exclusive");
    if(explicit_nodes){if(!explicit_nodes.IsSequence()||explicit_nodes.size()==0)throw std::invalid_argument("hotspots must be non-empty");for(const auto &h:explicit_nodes)add(h,h["center"][0].as<double>(),h["center"][1].as<double>(),nullptr);}
    else { const auto count=n["count"]?n["count"].as<std::size_t>():0U;if(count==0)throw std::invalid_argument("gaussian_hotspots requires positive count");RandomStream random(seed);for(std::size_t i=0;i<count;++i){Point2 p{};bool ok=false;for(std::size_t r=0;r<10000U;++r){p={domain.min_x+(domain.max_x-domain.min_x)*random.uniform_open(),domain.min_y+(domain.max_y-domain.min_y)*random.uniform_open()};if(contains(domain.polygon,p)){ok=true;break;}}if(!ok)throw std::runtime_error("hotspot center rejection sampling exhausted");const auto px=normalized_?(p.x-domain.min_x)/(domain.max_x-domain.min_x):p.x;const auto py=normalized_?(p.y-domain.min_y)/(domain.max_y-domain.min_y):p.y;add(n,px,py,&random);}}
  }
  double evaluate(double x,double y,double u,double v,const std::vector<double>&)const override{if(normalized_){x=u;y=v;}double sum=0;for(const auto &h:spots_){const auto dx=x-h.x,dy=y-h.y,c=std::cos(h.rotation),s=std::sin(h.rotation),a=c*dx+s*dy,b=-s*dx+c*dy;sum+=h.amplitude*std::exp(-0.5*(a*a/(h.sx*h.sx)+b*b/(h.sy*h.sy)));}return sum;}
private:
  void add(const YAML::Node &n,double x,double y,RandomStream *random){double sx=1,sy=1;const auto sigma=n["sigma"];if(sigma&&sigma.IsSequence()){sx=sigma[0].as<double>();sy=sigma[1].as<double>();}else{sx=scalar(n,"sigma",1,random);const auto aspect=scalar(n,"aspect_ratio",1,random);sy=sx*aspect;}const auto rotation=scalar(n,"rotation",0,random),amplitude=scalar(n,"amplitude",1,random);if(!(sx>0&&sy>0)||!std::isfinite(x+y+sx+sy+rotation+amplitude))throw std::invalid_argument("invalid gaussian hotspot");spots_.push_back({x,y,sx,sy,rotation,amplitude});}
  bool normalized_{};std::vector<Hotspot> spots_;
};
} // namespace

void ModifierFieldFactory::register_generator(std::string name,Creator creator){if(name.empty()||!creator)throw std::invalid_argument("modifier field registration requires name and creator");if(!creators_.emplace(std::move(name),std::move(creator)).second)throw std::invalid_argument("modifier field name already registered");}
std::unique_ptr<ModifierField> ModifierFieldFactory::create(std::string_view name,const YAML::Node &node,const ModifierDomain &domain,std::uint64_t seed)const{const auto it=creators_.find(std::string(name));if(it==creators_.end())throw std::invalid_argument("unknown modifier field type: "+std::string(name));auto result=it->second(node,domain,seed);if(!result)throw std::runtime_error("modifier field creator returned null");return result;}
ModifierFieldFactory make_builtin_modifier_field_factory(){ModifierFieldFactory f;f.register_generator("expression",[](const YAML::Node&n,const ModifierDomain&,std::uint64_t){return std::make_unique<ExpressionField>(n);});f.register_generator("fractal_noise",[](const YAML::Node&n,const ModifierDomain&,std::uint64_t s){return std::make_unique<NoiseField>(n,s);});f.register_generator("gaussian_hotspots",[](const YAML::Node&n,const ModifierDomain&d,std::uint64_t s){return std::make_unique<HotspotField>(n,d,s);});return f;}

ModifierFieldSet ModifierFieldSet::compile(const YAML::Node &node,const Polygon2 &domain,std::uint64_t world_seed,GenerationKey key,const ModifierFieldFactory &factory){ModifierFieldSet result;result.domain_.polygon=domain;result.domain_.min_x=result.domain_.min_y=std::numeric_limits<double>::infinity();result.domain_.max_x=result.domain_.max_y=-std::numeric_limits<double>::infinity();for(const auto&p:domain.vertices){result.domain_.min_x=std::min(result.domain_.min_x,p.x);result.domain_.min_y=std::min(result.domain_.min_y,p.y);result.domain_.max_x=std::max(result.domain_.max_x,p.x);result.domain_.max_y=std::max(result.domain_.max_y,p.y);}if(!node)return result;if(!node.IsMap())throw std::invalid_argument("modifier_fields must be a map");for(const auto &entry:node){const auto name=entry.first.as<std::string>();if(name.empty()||!(std::isalpha(static_cast<unsigned char>(name[0]))||name[0]=='_')||!std::all_of(name.begin()+1,name.end(),[](char c){return std::isalnum(static_cast<unsigned char>(c))||c=='_';})||name=="x"||name=="y"||name=="u"||name=="v"||name=="pi"||name=="e")throw std::invalid_argument("invalid or reserved modifier field name: "+name);result.names_.push_back(name);}std::sort(result.names_.begin(),result.names_.end());if(std::adjacent_find(result.names_.begin(),result.names_.end())!=result.names_.end())throw std::invalid_argument("duplicate modifier field name");std::unordered_map<std::string,std::size_t> indices;for(std::size_t i=0;i<result.names_.size();++i)indices.emplace(result.names_[i],i);for(const auto &name:result.names_){const auto definition=node[name];if(!definition.IsMap()||!definition["gentype"])throw std::invalid_argument("modifier field requires gentype: "+name);const auto offset=definition["seed_offset"]?definition["seed_offset"].as<std::uint64_t>():0U;const auto seed=mix64(world_seed^key.value^hash_name(name)^offset);result.fields_.push_back(factory.create(definition["gentype"].as<std::string>(),definition,result.domain_,seed));}std::vector<int> state(result.fields_.size());std::function<void(std::size_t)>visit=[&](std::size_t i){if(state[i]==1)throw std::invalid_argument("modifier field dependency cycle");if(state[i]==2)return;state[i]=1;for(const auto&name:result.fields_[i]->dependencies()){const auto it=indices.find(name);if(it==indices.end())throw std::invalid_argument("unknown modifier field: "+name);visit(it->second);}state[i]=2;result.evaluation_order_.push_back(i);};for(std::size_t i=0;i<result.fields_.size();++i)visit(i);for(auto &field:result.fields_)field->bind(indices);return result;}
std::size_t ModifierFieldSet::resolve(std::string_view name)const{const auto it=std::lower_bound(names_.begin(),names_.end(),name);if(it==names_.end()||*it!=name)throw std::invalid_argument("unknown modifier field: "+std::string(name));return static_cast<std::size_t>(it-names_.begin());}
void ModifierFieldSet::evaluate(Point2 p,std::vector<double>&buffer)const{buffer.resize(fields_.size());const auto u=(p.x-domain_.min_x)/(domain_.max_x-domain_.min_x),v=(p.y-domain_.min_y)/(domain_.max_y-domain_.min_y);for(const auto i:evaluation_order_){buffer[i]=fields_[i]->evaluate(p.x,p.y,u,v,buffer);if(!std::isfinite(buffer[i]))throw std::runtime_error("modifier field produced non-finite value at crop position");}}
GrayscaleImage render_modifier_field(const ModifierFieldSet&fields,std::size_t index,std::size_t width,std::size_t height,std::optional<std::pair<double,double>> range){if(width==0||height==0||index>=fields.size()||width>std::numeric_limits<std::size_t>::max()/height)throw std::invalid_argument("invalid modifier render request");std::vector<double> samples(width*height,std::numeric_limits<double>::quiet_NaN()),buffer;double lo=std::numeric_limits<double>::infinity(),hi=-lo;const auto &d=fields.domain();const auto width_value=static_cast<double>(width),height_value=static_cast<double>(height);for(std::size_t row=0;row<height;++row)for(std::size_t col=0;col<width;++col){Point2 p{d.min_x+(static_cast<double>(col)+0.5)*(d.max_x-d.min_x)/width_value,d.max_y-(static_cast<double>(row)+0.5)*(d.max_y-d.min_y)/height_value};if(contains(d.polygon,p)){fields.evaluate(p,buffer);const auto value=buffer[index];samples[row*width+col]=value;lo=std::min(lo,value);hi=std::max(hi,value);}}if(range){lo=range->first;hi=range->second;if(!std::isfinite(lo+hi)||hi<lo)throw std::invalid_argument("invalid modifier render range");}GrayscaleImage image{width,height,std::vector<std::uint8_t>(width*height)};for(std::size_t i=0;i<samples.size();++i)if(std::isfinite(samples[i]))image.pixels[i]=lo==hi?128U:static_cast<std::uint8_t>(std::llround(255.0*std::clamp((samples[i]-lo)/(hi-lo),0.0,1.0)));return image;}
} // namespace cropsim::generators
