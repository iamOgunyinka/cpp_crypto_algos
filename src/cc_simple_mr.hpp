#pragma once

#include "enum.hpp"
#include "order_executor.hpp"
#include "utils.hpp"
#include "wscc_trade_stream.hpp"

#include <fmt/core.h>

namespace profitview
{

template<std::floating_point Float = double, std::integral Int = int>
class CcSimpleMR
    : public TradeStream
    , private ccapi::CcTradeHandler
{
public:
    CcSimpleMR(
        const std::string trade_stream_name,
        OrderExecutor* executor,
        Int lookback,
        Float reversion_level,
        Float base_quantity,
        const std::string& csv_name = "SimpleMR.csv")
        : ccapi::CcTradeHandler(trade_stream_name)
        , lookback_{lookback}
        , reversion_level_{reversion_level}
        , base_quantity_{base_quantity}
        , executor_{executor}
        , csv_{csv_name}
        , csv_writer_{csv_}
    {}

    void onStreamedTrade(TradeData const& trade_data) override
    {
        util::print_trade_data(trade_data);

        auto& [elements, prices]{counted_prices_[trade_data.symbol]};

        prices.emplace_back(trade_data.price);

        if (prices.size() > lookback_)
        {
            auto mean{util::ma(prices)};

            auto std_reversion{reversion_level_ * util::stdev(prices, mean, lookback_)};

            prices.pop_front();
            
            bool 
                sell_signal{trade_data.price > mean + std_reversion},
                buy_signal {trade_data.price < mean - std_reversion};

            if (sell_signal)
            {    
                executor_->new_order(trade_data.symbol, Side::Sell, base_quantity_, OrderType::Market);
            }
            else if (buy_signal)
            {    
                executor_->new_order(trade_data.symbol, Side::Buy, base_quantity_, OrderType::Market);
            }
            
            csv_writer_.write(
                trade_data.symbol,
                trade_data.price,
                toString(trade_data.side).data(),
                trade_data.size,
                trade_data.source,
                trade_data.time,
                mean,
                std_reversion,
                buy_signal ? "Buy" : (sell_signal ? "Sell" : "No trade"));
        }
    }

    void subscribe(const std::string& market, const std::vector<std::string>& symbol_list)
    {
        CcTradeHandler::subscribe(market, symbol_list);
    }

private:
    const Int lookback_;

    const Float reversion_level_;    // Multiple of stdev
    Float base_quantity_;

    std::map<std::string, std::pair<Int, std::deque<Float>>> counted_prices_;

    OrderExecutor* executor_;

    std::ofstream csv_;
    util::CsvWriter csv_writer_;
};
}    // namespace profitview