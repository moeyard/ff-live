const path = require("path");
const server = 'localhost';
module.exports = {
  entry: './src/app.js',
  output: {
    path : path.resolve(__dirname, "dist"),
    filename: "app.js"
  },
  devServer :{
    host : 'localhost',
    port : '8080',
    proxy: {
      "/stream": {target:"ws://" + server + ":3000",ws:true}
    }
  },
  module : {
    rules : [
      { 
        test: /\.js[x]?$/, 
        loader: 'babel-loader', 
        exclude: /node_modules/ 
      },
      {
        test: /\.scss$/, 
        loader: 'style-loader!css-loader!sass-loader', 
      },
      {
        test: /\.css$/, 
        loader: 'style-loader!css-loader', 
      }

    ]
  }
}
