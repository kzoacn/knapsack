// Independent-review reproduction; see docs/independent-review.md.
#include "common.hpp"
#include "hinted.hpp"
#include <bit>
#include "coloring.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <string>
using namespace knapsack::detail;

void run(std::size_t k, bool finite) {
    std::size_t n = 16*k*k;
    HintSets hints(n);
    std::vector<Score> input(n, unreachable);
    std::vector<std::size_t> star_source(k);
    std::size_t row=0;
    for (std::size_t a=0;a<k;++a) for(std::size_t b=a+1;b<k;++b) {
        hints[row]={a,b};
        if(a==0) star_source[b]=row;
        ++row;
    }
    auto family=isolating_family(hints,k);
    std::vector<std::vector<std::size_t>> roots(family.size());
    for(std::size_t j=0;j<family.size();++j) {
        const auto& c=family[j];
        for(auto h:c.isolated) if(h<k-1 && c.colors[h+1]==1) roots[j].push_back(h+1);
        std::cerr<<"k="<<k<<" j="<<j<<" isolated="<<c.isolated.size()<<" roots="<<roots[j].size()<<" common="<<c.colors[0]<<"\n";
    }
    const std::size_t alpha=k;
    std::vector<ConcaveFunction> fs(k);
    for(std::size_t f=0;f<k;++f) {fs[f].weight=f+1;fs[f].tail_slope=-1;fs[f].max_count=n/fs[f].weight;}
    for(std::size_t t=1;t<=n;++t) fs[0].prefix.push_back(-Score(alpha)*t*t);
    fs[0].tail_slope=-Score(alpha)*(2*n+1);
    for(std::size_t j=0;j<roots.size();++j) {
        const auto count=roots[j].size();
        if(count==0) continue;
        const Score beta=alpha/count;
        const auto spacing=n/count;
        Score intercept=0;
        for(std::size_t l=0;l<count;++l) {
            const auto f=roots[j][l], z=star_source[f];
            if(l) {
                const auto previous=star_source[roots[j][l-1]];
                intercept-=Score(alpha)*Score(z-previous)*Score((2*l-1)*spacing);
            }
            input[z]=Score(alpha)*z*z+intercept;
            auto& q=fs[f];
            const Score w=q.weight;
            const Score linear=2*w*(Score(alpha)*z-(Score(alpha)+beta)*Score(l*spacing));
            const auto maximum=n/q.weight;
            for(std::size_t t=1;t<=maximum;++t) q.prefix.push_back(linear*t-beta*w*w*t*t);
            q.tail_slope=linear-beta*w*w*Score(2*maximum+1);
        }
    }
    if(finite) for(std::size_t i=0;i<row;++i) if(input[i]==unreachable) input[i]=-100*Score(alpha)*n*n;
    knapsack::Statistics stats;
    const auto trace_limit=8*(family.size()+1)*n;
    auto result=extend_hinted(input,hints,fs,{n,trace_limit},stats);
    std::cout<<k<<","<<n<<","<<family.size()<<","<<stats.singleton_calls<<","<<stats.matrix_queries<<","<<stats.matrix_queries/(double(n)*std::log2(n))<<","<<stats.matrix_queries/(double(n)*std::log2(n)*std::log2(n))<<"\n";
    for (std::size_t i=0; i<n; ++i) {
        const auto& entry=result.entries[i];
        if (entry.profit==unreachable) continue;
        if (entry.origin>=n || input[entry.origin]==unreachable) throw std::logic_error("unreachable witness origin");
        auto weight=entry.origin;
        Score profit=input[entry.origin];
        std::size_t depth=0, previous=no_node;
        for (auto node=entry.head; node!=no_node; node=result.nodes[node].parent) {
            const auto& step=result.nodes[node];
            if (++depth>2 || previous==step.function || step.count>fs[step.function].max_count ||
                std::find(hints[entry.origin].begin(),hints[entry.origin].end(),step.function)==hints[entry.origin].end())
                throw std::logic_error("invalid multistar witness");
            previous=step.function;
            weight+=step.count*fs[step.function].weight;
            profit+=fs[step.function](step.count);
        }
        if (weight!=i || profit!=entry.profit) throw std::logic_error("multistar witness mismatch");
    }
    result={};
    for(std::size_t j=0;j<family.size();++j) {
        std::vector<ExtensionEntry> current(n);
        std::vector<std::size_t> singles(n,no_node),by_handle(n,no_node);
        std::vector<ExtensionNode> nodes;
        for(auto h:family[j].isolated) current[h]={input[h],h,no_node};
        for(std::size_t c=0;c<4;++c) {
            for(auto h:family[j].isolated) for(auto f:hints[h])
                if(family[j].colors[f]==c) by_handle[h]=f;
            for(std::size_t i=0;i<n;++i) singles[i]=current[i].profit==unreachable?no_node:by_handle[current[i].origin];
            knapsack::Statistics local;
            current=extend_singletons(current,singles,fs,nodes,{n,trace_limit},local);
            if(c==0 && !roots[j].empty()) {
                const auto m=roots[j].size(), d=n/m, s=k/m;
                for(std::size_t t=k;t<n;++t) {
                    const auto l=std::min(m-1,(t+d/2-1)/d);
                    const auto z=star_source[roots[j][l]];
                    const Score expected=-Score(alpha)*t*t+2*Score(alpha)*z*t-Score(alpha)*s*d*l*l;
                    if(current[t].profit!=expected || current[t].origin!=z)
                        throw std::logic_error("Analytic first-color envelope mismatch");
                }
            }
            std::cerr<<"DETAIL j="<<j<<" c="<<c<<" queries="<<local.matrix_queries<<" source="<<local.peak_singleton_sources<<" perN="<<local.matrix_queries/double(n)<<"\n";
            std::fill(by_handle.begin(),by_handle.end(),no_node);
        }
    }
}

int main(int argc,char** argv) {
    try {
        if (argc==2 && std::string_view(argv[1])=="--help") {
            std::cout<<"Usage: knapsack_multistar_probe [K=64, a power of four in 4..256] [--ghost]\n";
            return 0;
        }
        const auto k=argc>1?knapsack::app::number(argv[1]):64;
        if (argc>3 || k<4 || k>256 || !std::has_single_bit(k) || std::countr_zero(k)%2 ||
            (argc==3 && std::string_view(argv[2])!="--ghost"))
            throw std::invalid_argument("expected a power of four in 4..256 and optional --ghost");
        std::cout<<"functions,rows,colorings,singleton_calls,matrix_queries,queries_over_n_log_n,queries_over_n_log_squared_n\n";
        run(static_cast<std::size_t>(k),argc!=3);
    } catch (const std::exception& error) {
        std::cerr<<"error: "<<error.what()<<'\n';
        return 2;
    }
}
